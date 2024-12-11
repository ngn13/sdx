DISTDIR = dist
PREFIX  = /usr

CFLAGS = -nostartfiles -nostdlib -ffreestanding -fpic -Werror -static-pie
LFLAGS = -static -pie --no-dynamic-linker -e __slibc_main -L../slibc/dist
LIBS   = -lslibc

CSRCS = $(shell find . -type f -name '*.c')
HSRCS = $(shell find . -type f -name '*.h')
OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))

all: $(DISTDIR) $(DISTDIR)/init

$(DISTDIR):
	mkdir -pv $@

$(DISTDIR)/init: $(OBJS)
	$(LD) -o $@ $(LFLAGS) $^

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r $(DISTDIR)

install:
	install -Dm755 $(DISTDIR)/init $(PREFIX)/init

.PHONY: clean install
