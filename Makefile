# Makefile for UNIX and MS-DOS (using DJGPP).

# Notes for DJGPP users (by Wolfgang Moser):
# 1. Remove -ansi from $(WARNFLAGS)
# 2. Activate -D_WRITE_PC64_DEFAULT_, unless you are going
#    to use cbmconvert in a system with long file name support.

DOCS = README

UNIXTARGETS = cbmconvert disk2zip zip2disk $(DOCS)
DOSTARGETS = cbmcnvrt.exe disk2zip.exe zip2disk.exe

OBJS = main.o util.o read.o write.o unlynx.o unark.o unarc.o t64.o \
       image.o archive.o
HDRS = util.h input.h output.h

# C compiler settings

#CC = checkergcc
CC = gcc

OPTFLAGS = -O9 -fomit-frame-pointer -funroll-loops -finline-functions
# DEBUGFLAGS = -g
WARNFLAGS = -Wall -ansi -pedantic -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Wwrite-strings -Wshadow -Wpointer-arith \
	-Wcast-align -Waggregate-return -Wnested-externs
# OTHER = -D_WRITE_PC64_DEFAULT_ # write PC64 files by default (for MS-DOS)

CFLAGS = $(OPTFLAGS) $(DEBUGFLAGS) $(WARNFLAGS) $(OTHER)

# Targets

all:
	@echo "Please use either 'make unix' or 'make dos'."
	@echo "DJGPP users may want to edit the Makefile."
	@exit 1

unix: $(UNIXTARGETS)

dos: $(DOSTARGETS)

clean:
	rm -f $(OBJS)

reallyclean: clean
	rm -f $(UNIXTARGETS) $(DOSTARGETS)

# Rules

cbmconvert: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(OBJS): $(HDRS)

README: README.html
	lynx -dump $< > $@

# DJGPP specific rules

.SUFFIXES: .exe

cbmcnvrt.exe: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.o.exe:
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
