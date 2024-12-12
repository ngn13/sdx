# sources, make sure slibc is compiled before everything else
SRCS  = slibc
SRCS += $(shell find . -maxdepth 1 -mindepth 1 -type d -not -name slibc)

# global compiler and linker flags for all the userspace binaries/libraries
CFLAGS  = -nostartfiles -nostdlib -ffreestanding -fPIC -fPIE -Werror
LFLAGS  = -pie --no-dynamic-linker -Bsymbolic
INCLUDE = -I../config

# export required variables
export CFLAGS
export LFLAGS
export INCLUDE

all:
	for src in $(SRCS); do \
		make -C $$src all; \
	done

install:
	for src in $(SRCS); do \
		make -C $$src install; \
	done

clean:
	for src in $(SRCS); do \
		make -C $$src clean; \
	done

.PHONY: clean install
