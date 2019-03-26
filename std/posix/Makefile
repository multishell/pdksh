# POSIX P1003.1 compatability
# $Id: Makefile,v 1.2 1992/04/25 08:22:14 sjg Exp $

SHELL = /bin/sh
MAKE  = make

#CONFIG = -D_SYSV
CONFIG = -D_BSD

LN = ln
RANLIB = ranlib		# For BSD systems
#RANLIB = echo Updated

LIB = ../libposix.a
INCL = ../h

CFLAGS = $(CCOPTS) -I$(INCL) $(CONFIG)

MISC =	Makefile 
HDRS =	io.h unistd.h fcntl.h dirent.h 
SYSHDRS = wait.h time.h times.h

SRCS =	unistd.c fcntl.c times.c dup2.c
OBJS =	$(LIB)(unistd.o) \
	$(LIB)(fcntl.o) \
	$(LIB)(times.o) \
	$(LIB)(dup2.o)

all:	$(LIB)

link:	$(HDRS)
	[ -d $(INCL) ] || mkdir $(INCL)
	[ -d $(INCL)/sys ] || mkdir $(INCL)/sys
	($(SHELL) ../mklinks $(INCL) $(HDRS))
	($(SHELL) ../mklinks $(INCL)/sys $(SYSHDRS))
	-if [ -r /usr/include/unistd.h ]; then $(RM) $(INCL)/unistd.h; fi 

$(LIB):	$(OBJS)
# if you make doesn't know how to put objects in libraries
# then simply make all the .o's and use the following line
#	ar r $@ $?
	$(RANLIB) $@

clean:
	-rm -f *.o *.out

clobber: clean
	-rm -f $(LIB)

fcntl.o: fcntl.h 

times.o: times.h 
