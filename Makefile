### User-configurable section begins

# Installation directory prefix for Debian GNU/Linux
DESTDIR =
# Installation directory prefix for other systems
PREFIX = $(DESTDIR)/usr
#PREFIX = $(DESTDIR)/usr/local

# Where to put binaries on 'make install'?
BINDIR = $(PREFIX)/bin
# Where to put manual pages on 'make installman'?
MANDIR = $(PREFIX)/share/man/man1

## Installation commands
RM = rm -f
INSTALLDIR = install -d
INSTALLDATA = install -m 444
INSTALLBIN = install -c

### User-configurable section ends

TARGETS = cbmconvert$(EXE) disk2zip$(EXE) zip2disk$(EXE)
MANPAGES = cbmconvert.1 disk2zip.1 zip2disk.1

SRCS = main.c util.c read.c write.c lynx.c unark.c unarc.c t64.c c2n.c \
       image.c archive.c
HDRS = util.h input.h output.h
OBJS = $(SRCS:.c=.o)

bugger:
	@echo Choose the target system, e.g.: make -f Makefile.unix
	@exit 1

real-all: $(TARGETS)

clean:
	$(RM) $(OBJS)

reallyclean: clean
	$(RM) $(TARGETS)

install: $(TARGETS)
	$(INSTALLDIR) $(BINDIR)
	$(INSTALLBIN) $(TARGETS) $(BINDIR)

installman: $(MANPAGES)
	$(INSTALLDIR) $(MANDIR)
	$(INSTALLDATA) $(MANPAGES) $(MANDIR)

depend: $(SRCS) $(HDRS)
	$(CC) -MM $(SRCS) > depend

cbmconvert$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

disk2zip$(EXE): disk2zip.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ disk2zip.c

zip2disk$(EXE): zip2disk.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ zip2disk.c

.phony: all clean reallyclean install installman

.SUFFIXES: .o .c .1 .dvi .pdf .txt

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
.1.dvi:
	groff -man -Tdvi $< > $@
.dvi.pdf:
	dvipdfm $<
.1.txt:
	groff -man -Tlatin1 $< | sed -e 's/.//g;' > $@

include depend
