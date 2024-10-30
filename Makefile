DISTDIR  = $(abspath dist)
ROOTDIR  = $(DISTDIR)/root
CROSSDIR = /opt/cross/bin

SRCS  = $(shell find . -type f -name '*.c')
SRCS += $(shell find . -type f -name '*.h')

USER_SRCS = $(shell find user/ -maxdepth 1 -mindepth 1 -type d)

LD = $(CROSSDIR)/x86_64-elf-ld
CC = $(CROSSDIR)/x86_64-elf-gcc

all: config/config.h $(ROOTDIR) kernel_bins user_bins

$(ROOTDIR): $(DISTDIR)
	mkdir -pv "$@/boot"
	mkdir -pv "$@/bin"
	mkdir -pv "$@/etc"

$(DISTDIR):
	mkdir -pv $@

##################################################
## build the kernel binary, see kernel/Makefile ##
##################################################
kernel_bins:
	make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C kernel all
	make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C kernel install

#####################################################
## build the disk image containing userspace files ##
#####################################################
user_bins:
	for src in $(USER_SRCS); do \
		make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C $$src all; \
		make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C $$src install; \
  done

##########################################################
## additional commands for configuration and formatting ##
##########################################################
config/config.h: config/config.json
	python3 scripts/config.py $^ $@

clean:
	rm -r $(DISTDIR)
	make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C kernel clean
	for src in $(USER_SRCS); do \
		make PREFIX="$(ROOTDIR)" LD="$(LD)" CC="$(CC)" -C $$src clean; \
  done

image:
	./scripts/image.sh

qemu:
	./scripts/qemu.sh

debug:
	./scripts/debug.sh

tools:
	./scripts/tools.sh

config:
	cp config/default.json config/config.json

format:
	clang-format -i -style=file $(SRCS)

.PHONY: clean config format image qemu debug tools
