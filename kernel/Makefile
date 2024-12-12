DISTDIR = dist
PREFIX  = /boot

# source files
HSRCS = $(shell find . -type f -name '*.h')
CSRCS = $(shell find . -type f -name '*.c')
SSRCS = $(shell find . -type f -name '*.S')

# target objects
OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))
OBJS += $(patsubst %.S,$(DISTDIR)/%.S.o,$(SSRCS))

# output and source directories
SRCDIRS = $(shell find . -type d -not -path './dist/*')
OUTDIRS = $(patsubst %,$(DISTDIR)/%,$(SRCDIRS))

# C compiler flags 
CFLAGS  = -nostdlib -nostartfiles -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2
CFLAGS += -Werror # warnings = errors
CFLAGS += -Wno-packed-bitfield-compat # disable useless ABI change warning

# ASM compiler flags
SFLAGS  = -g -nostdlib -nostartfiles -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2
SFLAGS += -Werror

# other flags
LFLAGS  = --nmagic
INCLUDE = -I inc -I ../config

all: $(DISTDIR) $(OUTDIRS) $(DISTDIR)/kernel

$(DISTDIR):
	mkdir -pv $@

$(OUTDIRS):
	mkdir -pv $(OUTDIRS)

$(DISTDIR)/kernel: $(OBJS)
	$(LD) $(LFLAGS) -o $@ -T ../config/link.ld $^

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

$(DISTDIR)/%.S.o: %.S $(HSRCS)
	$(CC) $(INCLUDE) $(SFLAGS) -c $< -o $@

clean:
	rm -rf "$(DISTDIR)"
	rm -f "$(DESTDIR)/$(PREFIX)/kernel"

install:
	install -Dm755 "$(DISTDIR)/kernel" "$(DESTDIR)/$(PREFIX)/kernel"

.PHONY: clean install
