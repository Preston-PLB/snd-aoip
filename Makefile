obj-m := snoip.o
snoip-y := snd_aes67.o

ccflags-y := -I $(src)/inc

KERN_DIR=/lib/modules/$(shell uname -r)/build/

default:
	$(MAKE) -C $(KERN_DIR) M=$$PWD modules
autoclean: clean default setup
setup:
	 bear -- $(MAKE)
clean:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) clean
help:
	$(MAKE) -C $(KERN_DIR) M=$(command -v "$1" >/dev/null 2>&1PWD) help
