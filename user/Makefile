# sources, make sure slibc is compiled before everything else
SRCS  = slibc
SRCS += $(shell find . -maxdepth 1 -mindepth 1 -type d -not -name slibc)

# global compiler and linker flags for all the userspace binaries/libraries
CFLAGS  = -nostartfiles -nostdlib -ffreestanding -fPIC -fPIE -Werror
SFLAGS  = $(CFLAGS) -D__ASSEMBLY__=1 # make sure __ASSEMBLY__ is defined in assembly sources
LFLAGS  = -pie --no-dynamic-linker -Bsymbolic
INCLUDE = -I../config -I../inc

# export required variables
export CFLAGS  # c compiler flags
export SFLAGS  # asm compiler flags
export LFLAGS  # linker flags
export INCLUDE # included directories

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
