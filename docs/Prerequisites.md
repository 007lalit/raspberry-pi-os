## Prerequisites

### 1. [Raspberry Pi 3 model b](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/) 

Older versions of Raspberry Pi are not going to work with this tutorial, because all lessons are designed to use 64 bit processor that supports ARMv8 architecture and such processor is only available in Raspberry Pi 3 model.

### 2. [USB to TTL serial cable](https://www.amazon.com/s/ref=nb_sb_noss_2?url=search-alias%3Daps&field-keywords=usb+to+ttl+serial+cable&rh=i%3Aaps%2Ck%3Ausb+to+ttl+serial+cable) 

After you get serial cable you need to test you connection. If you never done this before I recommend you to follow [this guide](https://cdn-learn.adafruit.com/downloads/pdf/adafruits-raspberry-pi-lesson-5-using-a-console-cable.pdf) It describes the process of connecting your raspberry Pi via serial cabel in great details. One thin that I can recomment you to do is to use your serial cable to power your Raspberry Pi. How to do this is described in the previous link.

### 3. Docker

Strictly speaking Docker is not a required dependency. It is just convinient to use docker to build source code of the lessons, especially for Mac and Windows users. Each lesson has `build.sh` script (or `build.bat` for windows users) This script uses docker to build source code of the lesson. Instructions how to install docker for you platform can be found on the [official docker website](https://docs.docker.com/engine/installation/)  

If for some reasons you want to avoid using Docker, you can install [make utility](http://www.math.tau.ac.il/~danha/courses/software1/make-intro.html) as well as  `aarch64-linux-gnu` toolchain. If you are using ubuntu you just need  to install `gcc-aarch64-linux-gnu` and `build-essential` packages.
