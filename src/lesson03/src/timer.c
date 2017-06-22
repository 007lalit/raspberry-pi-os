#include "utils.h"
#include "printf.h"
#include "peripherals/timer.h"

void timer_init ( void )
{
	PUT32(TIMER_LOAD,0x400); // Timer frequency = Clk/256 * 0x400 
	PUT32(TIMER_CONTROL,TIMER_CTRL_23BIT |
			TIMER_CTRL_ENABLE |
			TIMER_CTRL_INT_ENABLE |
			TIMER_CTRL_PRESCALE_256);
}

void handle_timer_irq( void ) 
{
	PUT32(TIMER_IRQ_CLEAR_ACK, 0x1);
	printf("Timer iterrupt received\n\r");
}
