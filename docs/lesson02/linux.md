## Lesson 2: Processor initialization (Linux)

Last time we stoped our exploration of linux kernel at [stext](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L116) function which is that entrypoint of `arm64` architecture. This time we are going to go a little bit deeper and find some similarities with the code that we have aready implemented in this lesson from Raspberry PI OS. 

this lesson might seem a little bit boring to you, because it consist moustly of the sidcussion of different ARM system registers and how they are used in linux kernl. But I still consider it very important for the following reasons:

1. It is important to usderstand the interface, that hardware provides to the software. Just by knowing this interface you will be able in many cases to deconstruct how a particular kernel feature is implemented and how software and hardware colaborate together to implement this feature.
1. Different options in the system register are usually related to enabling/disabling different hardware features. If you learn what different system registers an ARM processor have you will already have and idea what kind of features it provides.

Ok, not let's resume our investigation of the `stext` function.

```
ENTRY(stext)
	bl	preserve_boot_args
	bl	el2_setup			// Drop to EL1, w0=cpu_boot_mode
	adrp	x23, __PHYS_OFFSET
	and	x23, x23, MIN_KIMG_ALIGN - 1	// KASLR offset, defaults to 0
	bl	set_cpu_boot_mode_flag
	bl	__create_page_tables
	/*
	 * The following calls CPU setup code, see arch/arm64/mm/proc.S for
	 * details.
	 * On return, the CPU will be ready for the MMU to be turned on and
	 * the TCR will have been set.
	 */
	bl	__cpu_setup			// initialise processor
	b	__primary_switch
ENDPROC(stext)
``` 

### preserve_boot_args

[preserve_boot_args](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L136) function is responsible for saving some parameters, passed to the kernel by bootloader. 

```
preserve_boot_args:
	mov	x21, x0				// x21=FDT

	adr_l	x0, boot_args			// record the contents of
	stp	x21, x1, [x0]			// x0 .. x3 at kernel entry
	stp	x2, x3, [x0, #16]

	dmb	sy				// needed before dc ivac with
						// MMU off

	mov	x1, #0x20			// 4 x 8 bytes
	b	__inval_dcache_area		// tail call
ENDPROC(preserve_boot_args)
```

Accordingly to [kernel boot protocol](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt#L150), parameters are passed to the kernel in registers `x0 - x3`. `x0` contains physical address of device tree blob (dtb) in system RAM. `x1 - x3` for now are reserved for future usage. Whate this function is doing is copying the content of `x0 - x3` registers to the [boot_args](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/setup.c#L93) array and then [invalidate](https://developer.arm.com/products/architecture/a-profile/docs/den0024/latest/caches/cache-maintenance) coresponding cache line from the data cache. Cache maintainance in a multiprocessor system is a large topic on its own and we are going to skip it for now. For thous who are interested in this subject I can recommend to read [Caches](https://developer.arm.com/products/architecture/a-profile/docs/den0024/latest/caches) and [Multi-core processors](https://developer.arm.com/products/architecture/a-profile/docs/den0024/latest/multi-core-processors) chapters of the `ARM Programmer’s Guide`.

### el2_setup

Accordingly to the [boot protocol](https://github.com/torvalds/linux/blob/v4.14/Documentation/arm64/booting.txt#L159) kernel can be booted in either EL1 ro EL2. In the second case the kernel will have access to the virtualization eextensions and will be able to act as a host operating system. If we are lucky enough to be booted in EL2, [el2_setup](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/head.S#L386) function is responsible for configureing different parameters, assessible onlye at EL2, and dropping to EL1. This code should be more or less familiar to you, because it is similar to what we have done with the Raspberry PI OS in lesson 2. Now I am going to split this function into small parts and explain each part one by one.

```
	msr	SPsel, #1			// We want to use SP_EL{1,2}
``` 

Dedicated stack pointer will be used for both EL1 and EL2. Another option is to reuse stack pointer from EL0.

```
	mrs	x0, CurrentEL
	cmp	x0, #CurrentEL_EL2
	b.eq	1f
```

Only if current EL is EL branch to label `1`, otherwise not much is left to be done in this function.

```
	mrs	x0, sctlr_el1
CPU_BE(	orr	x0, x0, #(3 << 24)	)	// Set the EE and E0E bits for EL1
CPU_LE(	bic	x0, x0, #(3 << 24)	)	// Clear the EE and E0E bits for EL1
	msr	sctlr_el1, x0
	mov	w0, #BOOT_CPU_MODE_EL1		// This cpu booted in EL1
	isb
	ret
```

If it happens that we execute at EL1 `sctlr_el1` register is updated so that CPU works in either `big-endian` of `little-endian` mode depending on the value of [CPU_BIG_ENDIAN](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/Kconfig#L612) config setting. Then we just exit from the `el2_setup` function and return [BOOT_CPU_MODE_EL1](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/virt.h#L55) constant. Accordigly to [ARM64 Function Calling Conventions](http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf) return value should be placed into `x0` register (or `w0` in our case. You can htink about `w0` register as the first 32 bit of `x0`)

```
1:	mrs	x0, sctlr_el2
CPU_BE(	orr	x0, x0, #(1 << 25)	)	// Set the EE bit for EL2
CPU_LE(	bic	x0, x0, #(1 << 25)	)	// Clear the EE bit for EL2
	msr	sctlr_el2, x0
```

If it appears that we are booted in EL2 we are doing the same setup for EL2 ( note that `sctlr_el2` register is used instead of `sctlr_el1`)

```
#ifdef CONFIG_ARM64_VHE
	/*
	 * Check for VHE being present. For the rest of the EL2 setup,
	 * x2 being non-zero indicates that we do have VHE, and that the
	 * kernel is intended to run at EL2.
	 */
	mrs	x2, id_aa64mmfr1_el1
	ubfx	x2, x2, #8, #4
#else
	mov	x2, xzr
#endif
```

If [Virtualization Host Extensions (VHE)](https://developer.arm.com/products/architecture/a-profile/docs/100942/latest/aarch64-virtualization) are enabled via [ARM64_VHE](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/Kconfig#L926) config variable and are also supported by the host machine - `x2` then will contain non zero value. `x2` will be used to check whether `VHE` are enabled later in the same function..

```
	mov	x0, #HCR_RW			// 64-bit EL1
	cbz	x2, set_hcr
	orr	x0, x0, #HCR_TGE		// Enable Host Extensions
	orr	x0, x0, #HCR_E2H
set_hcr:
	msr	hcr_el2, x0
	isb
```

Here we set `hcr_el2` register. When we did the same job for Raspberry PI OS we used the same register to set 64 bit execution mode for EL!. This is exactly what is done in the first line of the provided code samepl. Also if `x2 != 0`, which means that VHE is available and kernel is configured to use it, `hcr_el2` is used to enable VHE.

```
	/*
	 * Allow Non-secure EL1 and EL0 to access physical timer and counter.
	 * This is not necessary for VHE, since the host kernel runs in EL2,
	 * and EL0 accesses are configured in the later stage of boot process.
	 * Note that when HCR_EL2.E2H == 1, CNTHCTL_EL2 has the same bit layout
	 * as CNTKCTL_EL1, and CNTKCTL_EL1 accessing instructions are redefined
	 * to access CNTHCTL_EL2. This allows the kernel designed to run at EL1
	 * to transparently mess with the EL0 bits via CNTKCTL_EL1 access in
	 * EL2.
	 */
	cbnz	x2, 1f
	mrs	x0, cnthctl_el2
	orr	x0, x0, #3			// Enable EL1 physical timers
	msr	cnthctl_el2, x0
1:
	msr	cntvoff_el2, xzr		// Clear virtual offset

```

Next pice of code is well explained in the comment above it. I have nothing to add.

```
#ifdef CONFIG_ARM_GIC_V3
	/* GICv3 system register access */
	mrs	x0, id_aa64pfr0_el1
	ubfx	x0, x0, #24, #4
	cmp	x0, #1
	b.ne	3f

	mrs_s	x0, SYS_ICC_SRE_EL2
	orr	x0, x0, #ICC_SRE_EL2_SRE	// Set ICC_SRE_EL2.SRE==1
	orr	x0, x0, #ICC_SRE_EL2_ENABLE	// Set ICC_SRE_EL2.Enable==1
	msr_s	SYS_ICC_SRE_EL2, x0
	isb					// Make sure SRE is now set
	mrs_s	x0, SYS_ICC_SRE_EL2		// Read SRE back,
	tbz	x0, #0, 3f			// and check that it sticks
	msr_s	SYS_ICH_HCR_EL2, xzr		// Reset ICC_HCR_EL2 to defaults

3:
#endif
```

Next code snippet is executed only if GICv3 is available and enabled. GIC stands for Generic Interrupt Controller. v3 version of the GIC specification adds a few features, that are particulary usefull in virtualization context. For example, viw GICv3 it becomes possible  to have LPIs (Locality-specific Peripheral Interrupt). Those interrups are routes via message bus, their configuration is held in tables in memory.  

The provided code is responsible for enablig SRE (System Register Interface) This step must be done before we will be able to use `ICC_*_ELn` registers and take advanages of GICv3 features. 

```
	/* Populate ID registers. */
	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1
```

`midr_el1` and `mpidr_el1` are readonly registers from the Identification registers group. They provide various information about processor manufacturer, processor architecture name, number of cores and some other. It is posible to change this information for all readers that tries to access it from EL1. Here we populate `vpidr_el2` and ` vmpidr_el2` with identical values, so this information is the same whether you tries to access it from EL1 or from higer exception levels.

```
#ifdef CONFIG_COMPAT
	msr	hstr_el2, xzr			// Disable CP15 traps to EL2
#endif
```

When processor is executing in 32 bit execution mode there is a concept of "coprocessor". Coprocessor can be used to access information, that in 64 bit execution mode is typicaly accessed via system registers. You can read about what exactly is accesible via coprocessor [in hte official documentation](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0311d/I1014521.html). `msr	hstr_el2, xzr` instruction allows to use coprocessor from lower exception levels. This make sense to do only when compatibility mode is enabled (in this mode kernel can run 32 bit user applications with 64 bit kernl) 

```
	/* EL2 debug */
	mrs	x1, id_aa64dfr0_el1		// Check ID_AA64DFR0_EL1 PMUVer
	sbfx	x0, x1, #8, #4
	cmp	x0, #1
	b.lt	4f				// Skip if no PMU present
	mrs	x0, pmcr_el0			// Disable debug access traps
	ubfx	x0, x0, #11, #5			// to EL2 and allow access to
4:
	csel	x3, xzr, x0, lt			// all PMU counters from EL1

	/* Statistical profiling */
	ubfx	x0, x1, #32, #4			// Check ID_AA64DFR0_EL1 PMSVer
	cbz	x0, 6f				// Skip if SPE not present
	cbnz	x2, 5f				// VHE?
	mov	x1, #(MDCR_EL2_E2PB_MASK << MDCR_EL2_E2PB_SHIFT)
	orr	x3, x3, x1			// If we don't have VHE, then
	b	6f				// use EL1&0 translation.
5:						// For VHE, use EL2 translation
	orr	x3, x3, #MDCR_EL2_TPMS		// and disable access from EL1
6:
	msr	mdcr_el2, x3			// Configure debug traps
```

This pice of code is responsible for configureing `mdcr_el2` Monitor Debug Configuration Register (EL2). This register is responsible for configuring different debug traps related to the virtualization extension.  I am going to leave the details of this code block unexplained, because debug and tracing are a little bit out of scope for our discussion. If you are interested in details I can ecomend you to read the description of `mdcr_el2` register on page `2114` of the [AArch64-Reference-Manual](https://developer.arm.com/docs/ddi0487/latest/arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile)

```
	/* Stage-2 translation */
	msr	vttbr_el2, xzr
```

When your OS is used as a hypervisor it should provide complete memory isolation for its guest OSes. Stage 2 virtual memory translation is used exactly for this purpose: each guest OS think that it owns all system memory, though in reality each memory access is mapped to the physical memory by stage 2 translation. `vttbr_el2`  holds the base address of the translation table for the stage 2 translation.  For now stage 2 translation is disabled, and `vttbr_el2` should be set to 0.

```
	cbz	x2, install_el2_stub

	mov	w0, #BOOT_CPU_MODE_EL2		// This CPU booted in EL2
	isb
	ret
```

First `x2` is compared to `0` to check whether VHE is enabled. If yes - jump to `install_el2_stub` label, otherwise record that CPU is booted in EL2 mode and exit from `el2_setup` function.  In later case processor contiues to operate in EL2 mode and EL1 will not be used at all.

```
install_el2_stub:
	/* sctlr_el1 */
	mov	x0, #0x0800			// Set/clear RES{1,0} bits
CPU_BE(	movk	x0, #0x33d0, lsl #16	)	// Set EE and E0E on BE systems
CPU_LE(	movk	x0, #0x30d0, lsl #16	)	// Clear EE and E0E on LE systems
	msr	sctlr_el1, x0

```

If we reach this point it means that we are going to switch to EL1 soon, so early EL1 initialization need to be done here.  The copied code snippet is responsible for `sctlr_el1` (System Control Register) initialization. We alredy did the same [here](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson02/src/boot.S#L18)

```
	/* Coprocessor traps. */
	mov	x0, #0x33ff
	msr	cptr_el2, x0			// Disable copro. traps to EL2
```

This code allows EL1 to access `cpacr_el1` register and as a result to control access to Trace, Floating-point, and Advanced SIMD functionality.

```
	/* Hypervisor stub */
	adr_l	x0, __hyp_stub_vectors
	msr	vbar_el2, x0
```

We don't plan to use EL2 now, though for some functionality it is unavoidable. We need it, for example, to implement [kexec](https://linux.die.net/man/8/kexec) system call, that enables you to load and boot into another kernel from the currently running kernel. 

[_hyp_stub_vectors](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/kernel/hyp-stub.S#L33  holds the addresses of all exception handlers for EL2. We are going to implement similar functionality for EL1 in the next lesson, wnen we will talk about interrupts and exception handling in details.

```
	/* spsr */
	mov	x0, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT |\
		      PSR_MODE_EL1h)
	msr	spsr_el2, x0
	msr	elr_el2, lr
	mov	w0, #BOOT_CPU_MODE_EL2		// This CPU booted in EL2
	eret
```

Finally we need to initialize processor state at EL1 and wsitch exception levels. We already did it for [rpiOS](https://github.com/s-matyukevich/raspberry-pi-os/blob/master/src/lesson02/src/boot.S#L27-L33) so I am not going to explain the details of this code. 

The only new thing here is the way how `elr_el2` is initialized. `lr` or Link Register is an alias for `x30`. Whenever you execute `br` (Branch Link) instruction it is automatically populated with the curret execution address. This is usually used by `ret` instruction, so it new to which point to return. In our case `lr` will point to [this](instruction) and because of the way how we initialized  `elr_el2` this is also the instruction from which execution is going to be resumed after swithcing to EL1.

### Processor initialization at EL1

Now we are bach to the `stext` function. Next few lines are not very important for us, but I wand to explin them for the sake of completenes.

```
	adrp	x23, __PHYS_OFFSET
	and	x23, x23, MIN_KIMG_ALIGN - 1	// KASLR offset, defaults to 0
```
[KASLR](https://lwn.net/Articles/569635/) of Kernel address space layout randomization is a technique that allows to place the kernel itself at a random addres in memory. This is required only for security reasons. For more information you can read the provided link.

```
	bl	set_cpu_boot_mode_flag
```

Here CPU boot mode is saved into [__boot_cpu_mode](https://github.com/torvalds/linux/blob/v4.14/arch/arm64/include/asm/virt.h#L74) variable. The code that does this is ery similar `preserve_boot_args` function that we explored previously.  

```
	bl	__create_page_tables
	bl	__cpu_setup			// initialise processor
	b	__primary_switch
```

The last 3 functions are very implrtant, but they all are related to vitual memory management we are going to postpone there detaild exploration untill the lesson 6. For now I just want to brefely describe there purposes.
* `__create_page_tables` - as its name stands this one is responsible for creating Page Tables.
* `__create_page_tables` - initialize various processor settings, mostly specific for virtual memory management.
* `__primary_switch` - enable MMU annd jump to [start_kernel](https://github.com/torvalds/linux/blob/v4.14/init/main.c#L509) function, which is architecture independent starting point.
 
