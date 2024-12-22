# dirs
DISTDIR = dist
PREFIX  = /

# extra flags
INCLUDE += -I./inc/

# source files & target objects
CSRCS = $(shell find . -type f -name '*.c' -not -name 'crt0.c')
HSRCS = $(shell find . -type f -name '*.h')
OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))

all: $(DISTDIR) $(DISTDIR)/libc.a $(DISTDIR)/crt0.o

$(DISTDIR):
	mkdir -pv $@

$(DISTDIR)/libc.a: $(OBJS)
	ar rcs $@ $^

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(DISTDIR)/crt0.o: crt0.S $(HSRCS)
	$(CC) -c $(SFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -rf $(DISTDIR)

install: ;

.PHONY: clean install
