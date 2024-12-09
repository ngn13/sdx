DISTDIR = dist
PREFIX  = /usr

CFLAGS = -nostartfiles -nostdlib -ffreestanding -fpic -Werror
LFLAGS = -pie --no-dynamic-linker

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
