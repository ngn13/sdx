# dirs 
DISTDIR = dist
PREFIX  = /

INCLUDE += -I../slibc/inc
LIBS     = -L../slibc/dist -lc

# source files & target objects
CSRCS = $(shell find . -type f -name '*.c')
HSRCS = $(shell find . -type f -name '*.h')
OBJS  = ../slibc/dist/crt0.o $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))

all: $(DISTDIR) $(DISTDIR)/init

$(DISTDIR):
	mkdir -pv $@

$(DISTDIR)/init: $(OBJS)
	$(LD) -o $@ $(LFLAGS) $^ $(LIBS)

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -rf $(DISTDIR)
	rm -f "$(DESTDIR)/$(PREFIX)/init"

install:
	install -Dm755 "$(DISTDIR)/init" "$(DESTDIR)/$(PREFIX)/init"

.PHONY: clean install
