# directory paths for different stuff
DESTDIR  = $(abspath dist)
CROSSDIR = /opt/cross/bin

# (all) source files
PYSRCS = $(shell find . -type f -name '*.py')
CSRCS  = $(shell find . -type f -name '*.c')
HSRCS  = $(shell find . -type f -name '*.h')
SSRCS  = $(shell find . -type f -name '*.S')

# compiler, linker and archiver
CC = $(CROSSDIR)/x86_64-elf-gcc
LD = $(CROSSDIR)/x86_64-elf-ld
AR = $(CROSSDIR)/x86_64-elf-ar

# exports
export DESTDIR
export CROSSDIR
export CC
export LD
export AR

all: config/config.h $(DESTDIR) kernel_bins user_bins

$(DESTDIR):
	mkdir -pv "$@/boot"
	mkdir -pv "$@/bin"
	mkdir -pv "$@/etc"

##################################################
## build the kernel binary, see kernel/Makefile ##
##################################################
kernel_bins:
	make -C kernel all
	make -C kernel install

#####################################################
## build the disk image containing userspace files ##
#####################################################
user_bins:
	make -C user all
	make -C user install

#################################################################
## create the configuration header from the configuration JSON ##
#################################################################
config/config.h: config/config.json
	python3 scripts/config.py $^ $@

##########################################################
## additional commands for configuration and formatting ##
##########################################################
clean:
	make -C kernel clean
	make -C user clean
	rm -f "$(DESTDIR)/sdx.img"

image:
	make
	./scripts/image.sh

qemu:
	./scripts/qemu.sh --serial-log

test:
	./scripts/test.sh

debug:
	./scripts/debug.sh

tools:
	./scripts/tools.sh

config:
	cp config/default.json config/config.json

format:
	black $(PYSRCS)
	clang-format -i -style=file $(CSRCS)
	clang-format -i -style=file $(HSRCS)
	# TODO: find a good ASM formatter and add it here

.PHONY: clean image qemu debug tools config format
