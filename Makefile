obj-m := i2c-dev.o
#obj-m := i2c-scan.o
#CFLAGS_i2c-dev.o := -DDEBUG

KDIR =  /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

default:
	#$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) SUBDIRS=$(PWD) modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	$(CC) i2cdetect.c -o i2cdetect 

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

#make ARCH=arm CROSS_COMPILE=arm-linux-
