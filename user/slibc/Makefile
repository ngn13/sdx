# dirs
DISTDIR = dist
PREFIX  = /

# extra flags
INCLUDE += -I./inc/

# source files
SSRCS = $(shell find . -type f -name '*.S' -not -name 'crt0.S')
CSRCS = $(shell find . -type f -name '*.c')
HSRCS = $(shell find . -type f -name '*.h')

# target objects
OBJS  = $(patsubst %.c,$(DISTDIR)/%.c.o,$(CSRCS))
OBJS += $(patsubst %.S,$(DISTDIR)/%.S.o,$(SSRCS))

all: $(DISTDIR) $(DISTDIR)/libc.a $(DISTDIR)/crt0.o

$(DISTDIR):
	mkdir -pv $(DISTDIR)

$(DISTDIR)/libc.a: $(OBJS)
	ar rcs $@ $^

$(DISTDIR)/crt0.o: $(DISTDIR)/crt0.S.o
	cp $^ $@

$(DISTDIR)/%.c.o: %.c $(HSRCS)
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(DISTDIR)/%.S.o: %.S $(HSRCS)
	$(CC) -c $(SFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -rf $(DISTDIR)

install: ;

.PHONY: clean install
