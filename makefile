CFLAGS+=-Wall -pedantic -Os

DESTDIR:=
SBINDIR:=$(DESTDIR)/sbin
MANDIR:=$(DESTDIR)/usr/share/man

all: picod pico-i2cd

clean:
	rm -f picod pico-i2cd

doxygen:: doxyfile
	doxygen $<

install: all
	mkdir -p $(SBINDIR) || true
	mkdir -p $(MANDIR)/man1 || true
	install picod $(SBINDIR)
	install pico-i2cd $(SBINDIR)
	install picod.1 $(MANDIR)/man1
	install pico-i2cd.1 $(MANDIR)/man1
