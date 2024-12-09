DISTDIR = dist
PREFIX  = /

HSRCS = $(shell find . -type f -name '*.h')
CSRCS = $(shell find . -type f -name '*.c')
SSRCS = $(shell find . -type f -name '*.S')

OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))
OBJS += $(patsubst %.S,$(DISTDIR)/%.S.o,$(SSRCS))

SRCDIRS = $(shell find . -type d -not -path './dist/*')
OUTDIRS = $(patsubst %,$(DISTDIR)/%,$(SRCDIRS))

CFLAGS  = -nostdlib -nostartfiles -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Werror
SFLAGS  = -g -nostdlib -nostartfiles -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Werror
LFLAGS  = --nmagic
INCLUDE = -I inc -I ../config

# remvoed warnings
CFLAGS += -Wno-packed-bitfield-compat # disable useless ABI change warning

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
	rm -r "$(DISTDIR)"

install:
	install -Dm755 "$(DISTDIR)/kernel" "$(PREFIX)/boot/kernel"

.PHONY: clean install
