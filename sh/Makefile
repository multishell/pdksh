# PD Bourne/Korn Shell
# $Id: Makefile,v 1.2 1992/04/25 08:33:03 sjg Exp $

SHELL = /bin/sh
MAKE  = make

LN    = ln -s

# You _can_ build this shell without the ../std tree if your
# system provides a sufficiently POSIX environment, or if your
# BSD system is a Sun or close.  If not try ../std.

# gcc is good value on most mc68k's and sun386's if nothing else.
# if you don't have gcc cc should do
# CC=gcc -pipe
CC=gcc -pipe 

# The following are the defintions used (or thereabouts) 
# to build ksh without ../std
#
# sun386 SunOS 4.0.2, sun3 SunOS 4.0.3
# CONFIG=-D_BSD
# XOPTS=-DNOSTDHDRS
# copy these from ../std/stdc or ensure they are in libstdc.a
# XOBJS = memmove.o strstr.o 
#
# sun3  SunOS 4.1.1
# CONFIG=-D_BSD
# XOPTS=-DNOSTDHDRS
# copy these from ../std/stdc or ensure they are in libstdc.a
# XOBJS = memmove.o
#
# sun4c (sparc) SunOS 4.1.1
# CC=cc -pipe	# don't use gcc
# CONFIG=-D_BSD
# XOPTS=-DNOSTDHDRS
# copy these from ../std/stdc or ensure they are in libstdc.a
# XOBJS = memmove.o
#
# Bull DPX/2 B.O.S. 2.00.45
# CC=gcc -ansi
# CONFIG=-D_POSIX_SOURCE
# XOPTS=
# XOBJS=
#
# Minix-386 1.5.10 with estdio
# CONFIG= -D_BSD -D_MINIX -D_POSIX_SOURCE
# XOPTS=
# XOBJS=
#

#CONFIG= -D_SYSV
#CONFIG= -D_BSD -DHAVE_SYS_STDTYPES
CONFIG= -D_BSD


STD=../std
INCL=$(STD)/h
XINCL=-I$(INCL)
LDOPTS=-L$(STD)
XOBJS=

# use -O if you trust it :-)
#DBG=-g -O
CFLAGS = $(DBG) $(CONFIG) $(XINCL) $(XOPTS)

LDFLAGS = $(DBG) $(LDOPTS)

COMPATLIBS = -lstdc -lposix
#XLIBS = -lc_s
#XLIBS = -ldirent
LDLIBS = $(COMPATLIBS) $(XLIBS)

HDRS =	sh.h table.h expand.h lex.h tree.h tty.h trace.h
SRCS1 =	version.c main.c misc.c trap.c alloc.c io.c \
	syn.c lex.c edit.c emacs.c vi.c history.c tree.c 
SRCS2 =	exec.c jobs.c \
	c_sh.c c_ksh.c c_test.c getopts.c do_ulimit.c \
	var.c table.c eval.c expr.c mail.c sigact.c trace.c
SRCS =	Makefile $(HDRS) $(SRCS1) $(SRCS2) 

OBJS =	version.o main.o misc.o \
	syn.o lex.o edit.o emacs.o vi.o tree.o \
	exec.o jobs.o trap.o \
	c_sh.o c_ksh.o c_test.o \
	do_ulimit.o getopts.o expr.o history.o \
	var.o table.o alloc.o io.o eval.o mail.o sigact.o trace.o $(XOBJS)

ksh: $(OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f *.o *.out core

clobber: clean
	rm -f ksh

link:
	($(SHELL) ../std/mklinks ../std/h stdh.h)
	

depend:
	makedepend $(CFLAGS) $(SRCS1) $(SRCS2)

install:
	@echo "Try:"
	@echo "cp ksh /bin"
	@echo "strip /bin/ksh"
	@echo "chmod 555 /bin/ksh"

.c.s:
	$(CC) $(CFLAGS) -S -o $@ $<

asms: $(OBJS:.o=.s)


# DO NOT DELETE THIS LINE -- make depend depends on it.
# If this runs make out of memory, delete /usr/include lines.
alloc.o: alloc.c
alloc.o: config.h
alloc.o: sh.h
alloc.o: stdh.h
alloc.o: trace.h
c_ksh.o: c_ksh.c
c_ksh.o: config.h
c_ksh.o: sh.h
c_ksh.o: stdh.h
c_ksh.o: table.h
c_ksh.o: trace.h
c_sh.o: c_sh.c
c_sh.o: config.h
c_sh.o: lex.h
c_sh.o: sh.h
c_sh.o: stdh.h
c_sh.o: table.h
c_sh.o: trace.h
c_sh.o: tree.h
c_test.o: c_test.c
c_test.o: config.h
c_test.o: sh.h
c_test.o: stdh.h
c_test.o: trace.h
edit.o: config.h
edit.o: edit.c
edit.o: edit.h
edit.o: lex.h
edit.o: sh.h
edit.o: stdh.h
edit.o: trace.h
edit.o: tty.h
emacs.o: config.h
emacs.o: edit.h
emacs.o: emacs.c
emacs.o: expand.h
emacs.o: lex.h
emacs.o: sh.h
emacs.o: stdh.h
emacs.o: table.h
emacs.o: trace.h
emacs.o: tree.h
eval.o: config.h
eval.o: eval.c
eval.o: expand.h
eval.o: lex.h
eval.o: sh.h
eval.o: stdh.h
eval.o: table.h
eval.o: trace.h
eval.o: tree.h
exec.o: config.h
exec.o: exec.c
exec.o: lex.h
exec.o: sh.h
exec.o: stdh.h
exec.o: table.h
exec.o: trace.h
exec.o: tree.h
expr.o: config.h
expr.o: expr.c
expr.o: sh.h
expr.o: stdh.h
expr.o: table.h
expr.o: trace.h
getopts.o: config.h
getopts.o: getopts.c
getopts.o: sh.h
getopts.o: stdh.h
getopts.o: table.h
getopts.o: trace.h
history.o: config.h
history.o: history.c
history.o: lex.h
history.o: sh.h
history.o: stdh.h
history.o: trace.h
io.o: config.h
io.o: io.c
io.o: sh.h
io.o: stdh.h
io.o: trace.h
jobs.o: config.h
jobs.o: jobs.c
jobs.o: sh.h
jobs.o: stdh.h
jobs.o: trace.h
jobs.o: tree.h
lex.o: config.h
lex.o: expand.h
lex.o: lex.c
lex.o: lex.h
lex.o: sh.h
lex.o: stdh.h
lex.o: table.h
lex.o: trace.h
lex.o: tree.h
mail.o: config.h
mail.o: mail.c
mail.o: sh.h
mail.o: stdh.h
mail.o: table.h
mail.o: trace.h
main.o: config.h
main.o: lex.h
main.o: main.c
main.o: sh.h
main.o: stdh.h
main.o: table.h
main.o: trace.h
main.o: tree.h
memmove.o: memmove.c
memmove.o: stdh.h
misc.o: config.h
misc.o: expand.h
misc.o: misc.c
misc.o: sh.h
misc.o: stdh.h
misc.o: trace.h
strstr.o: stdh.h
strstr.o: strstr.c
syn.o: config.h
syn.o: expand.h
syn.o: lex.h
syn.o: sh.h
syn.o: stdh.h
syn.o: syn.c
syn.o: table.h
syn.o: trace.h
syn.o: tree.h
sigact.o: sigact.h sigact.c
table.o: config.h
table.o: sh.h
table.o: stdh.h
table.o: table.c
table.o: table.h
table.o: trace.h
times.o: times.c
trace.o: trace.c
trap.o: config.h
trap.o: sh.h
trap.o: stdh.h
trap.o: trace.h
trap.o: trap.c
tree.o: config.h
tree.o: sh.h
tree.o: stdh.h
tree.o: trace.h
tree.o: tree.c
tree.o: tree.h
do_ulimit.o: config.h
do_ulimit.o: sh.h
do_ulimit.o: stdh.h
do_ulimit.o: trace.h
do_ulimit.o: do_ulimit.c
var.o: config.h
var.o: expand.h
var.o: sh.h
var.o: stdh.h
var.o: table.h
var.o: trace.h
var.o: var.c
version.o: config.h
version.o: sh.h
version.o: stdh.h
version.o: trace.h
version.o: version.c
vi.o: config.h
vi.o: edit.h
vi.o: expand.h
vi.o: lex.h
vi.o: sh.h
vi.o: stdh.h
vi.o: table.h
vi.o: trace.h
vi.o: tree.h
vi.o: vi.c
# WARNING: Put nothing here or make depend will gobble it up!
