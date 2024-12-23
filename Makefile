obj-m := snoip.o
snoip-y := aes67.o rtp.o

ccflags-y := -I $(src)/include

CC=gcc
KERN_DIR=/lib/modules/$(shell uname -r)/build/

default:
	$(MAKE) -C $(KERN_DIR) M=$$PWD modules
autoclean: clean default setup
setup:
	./scripts/gen_compile_commands.sh $(KERN_DIR)
clean:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) clean
help:
	$(MAKE) -C $(KERN_DIR) M=$(command -v "$1" >/dev/null 2>&1PWD) help
