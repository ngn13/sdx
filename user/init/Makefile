DISTDIR = dist
PREFIX  = /usr

CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Werror
LFLAGS = --nmagic -r

CSRCS = $(shell find . -type f -name '*.c')
HSRCS = $(shell find . -type f -name '*.h')
OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))

all: $(DISTDIR) $(DISTDIR)/init

$(DISTDIR):
	mkdir -pv $@

$(DISTDIR)/init: $(OBJS)
	$(LD) $(LFLAGS) -o $@ $^

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r $(DISTDIR)

install:
	install -Dm755 $(DISTDIR)/init $(PREFIX)/init

.PHONY: clean install
