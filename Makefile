# PD Bourne/Korn Shell
# $Id: Makefile,v 1.2 1992/04/25 08:17:25 sjg Exp $

SHELL = /bin/sh
MAKE  = make
CC=gcc -pipe -g -O
LN=ln -s
#LN=ln
#CONFIG= -D_SYSV
CONFIG= -D_BSD 
#CONFIG= -D_BSD -DHAVE_SYS_STDTYPES
#CONFIG= -D_V7
#CONFIG= -D_ST		/* Atari ST */

MANPAGES = ksh.1
#MANDIR=/usr/catman/u_man/man1
#MANDIR=/usr/man/man1

#INSTALL=bsdinstall
INSTALL=install

all:	ksh

ksh:	libs
	( cd sh ; $(MAKE) 'CC=$(CC)' 'CONFIG=$(CONFIG)' $@ )

libs:
	( cd std ; $(MAKE) 'CC=$(CC)' 'CONFIG=$(CONFIG)' 'LN=$(LN)' libs )

install: sh/ksh
	( cd sh ; $(INSTALL) -s ksh $(DESTDIR)/bin )

sh/ksh:	ksh

inst-man: $(MANPAGES)
	$(INSTALL) -c -m 444 $(MANPAGES) $(MANDESTDIR)/man1

clean clobber:
	( cd std ; $(MAKE) $@ )
	( cd sh ; $(MAKE) $@ )
	-rm -f *.out

