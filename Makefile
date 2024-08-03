obj-m += aes67.o

CC=gcc
KERN_DIR=/lib/modules/$(shell uname -r)/build/

default:
	$(MAKE) -C $(KERN_DIR) M=$$PWD modules
clean:
	make -C $(KERN_DIR) M=$(PWD) clean
help:
	make -C $(KERN_DIR) M=$(PWD) help
