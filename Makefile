#  Makefile for hexer version 0.1.5

#  It might be helpful to read the `README'-file first.

#  -- Where? --
#  The following lines determine where the binaries and manual pages for
#  hexer are gonna live.
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/man/man1

#  -- Which terminal library? --
#  (It's probably save to leave the following lines unchanged.)
#
#  Use the following two lines, if you want to use the termcap-library.
TERMLIB ?=
TERMCAP ?= -ltermcap -lm
#
#  Uncomment these if you want to use curses.
#TERMLIB =
#TERMCAP = -lcurses
#
#  Uncomment the following two lines if you want to use the termlib code
#  supplied with the package (not recommended).
#TERMLIB = termlib.o
#TERMCAP =

#  If you want to add some system specific defines, it's probably more
#  appropriate to put them into `config.h'.
DEFINES = -DHEXER_VERSION=\"0.1.5\"

#  -- Which compiler? --
CC ?= cc
CFLAGS ?= -O
CFLAGS += $(DEFINES)
LDFLAGS ?=
LDLIBS = $(TERMCAP)
#
#  Uncomment the following lines if you want to use the GNU compiler.
#CC = gcc
#CFLAGS = -O6 $(DEFINES)
#LDFALGS =
#LDLIBS = $(TERMCAP)

#  -- Which installer? --
INSTALL ?= install
INSTALLBIN ?= $(INSTALL) -s
INSTALLMAN ?= $(INSTALL) -m 644
#
#  Uncomment these if you don't have an installer.
#INSTALL = cp
#INSTALLBIN = $(INSTALL)
#INSTALLMAN = $(INSTALL)

###  It shouldn't be necessary to change anything below this line.

HEXER = hexer
MYC = myc

CTAGS = ctags -tawf tags

OBJECTS = buffer.o tio.o edit.o main.o hexer.o readline.o regex.o port.o \
          exh.o set.o map.o signal.o util.o commands.o helptext.o calc.o \
	  $(TERMLIB)

all: config.check $(HEXER)

config.check:
	@{  if [ ! -f config.h ]; then \
	      echo '***' Please read the file README on how to configure and; \
	      echo '***' compile hexer.\  Thank you.; \
	      exit 1; \
	    fi; }

$(HEXER): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(MYC): calc.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DMYCALC=1 -o $@ calc.c -lm

bin2c: bin2c.c
	$(CC) $(CFLAGS) -o $@ bin2c.c

helptext.c: help.txt bin2c
	./bin2c -n helptext -o $@ help.txt

tags: *.c *.h
	-@{ \
	  echo Creating tags...; \
	  rm -f tags; \
	  for i in *.c *.h; do \
            echo $(CTAGS) $$i; \
	    $(CTAGS) $$i; \
	  done; \
	}

dep: depend

depend: *.c *.h
	-rm -f Makefile~
	sed '/\#\#\# DO NOT DELETE THIS LINE \#\#\#/q' \
	  < Makefile > Makefile~
	-echo >> Makefile~
	-echo '#' Dependencies: >> Makefile~
	-echo >> Makefile~
	@{ for i in *.c; do \
	      if [ "$$i" != 'termlib.c' ]; then \
	      echo $(CC) -MM $(CINCLUDE) $(CFLAGS) $$i '>>' Makefile~; \
	      $(CC) -MM $(CINCLUDE) $(CFLAGS) $$i >> Makefile~; \
	      fi \
	    done; }
	-echo >> Makefile~
	mv -f Makefile~ Makefile
	-touch depend

clean:
	rm -f $(HEXER) $(MYC) gen_testfile $(OBJECTS) bin2c
	rm -f helptext.c TESTFILE
	rm -f tags core *.bak

distclean: clean
	rm -f *~
	sed '/\#\#\# DO NOT DELETE THIS LINE \#\#\#/q' \
	  < Makefile > Makefile~
	echo >> Makefile~
	mv -f Makefile~ Makefile
	rm -f depend

install: all
	@{  echo installing $(HEXER) in $(BINDIR); \
	    $(INSTALLBIN) $(HEXER) $(BINDIR); \
	    echo installing $(HEXER).1 in $(MANDIR); \
	    $(INSTALLMAN) $(HEXER).1 $(MANDIR); \
	    if [ -f $(MYC) ]; then \
	      echo installing $(MYC) in $(BINDIR); \
	      $(INSTALLBIN) $(MYC) $(BINDIR); \
	      echo installing $(MYC).1 in $(MANDIR); \
	      $(INSTALLMAN) $(MYC).1 $(MANDIR); \
	    fi; }


### DO NOT DELETE THIS LINE ###

