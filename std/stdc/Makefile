# Standard C (ANSI) compatabilaty
# $Id: Makefile,v 1.3 1992/05/03 08:30:08 sjg Exp $

# edit this makefile such that only the functions that
# your systems doesn't have are provided.

SHELL = /bin/sh
MAKE  = make

#CONFIG = -D_SYSV
CONFIG = -D_BSD

LN = ln
RANLIB = ranlib		# For BSD systems
#RANLIB = echo Updated

LIB = ../libstdc.a
INCL = ../h

CFLAGS = $(CCOPTS) -I$(INCL) $(CONFIG)

MISC =	Makefile #stdio.h_std
HDRS =	limits.h stddef.h stdlib.h string.h time.h #stdarg.h 
SYSHDRS = types.h

SRCS =	strstr.c memmove.c stdio.c clock.c fprintf.c memchr.c \
	memcmp.c memcpy.c memset.c setvbuf.c sprintf.c \
	stdio.c strcat.c strchr.c strcmp.c strcpy.c strcspn.c \
	strerror.c strlen.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strrchr.c strspn.c strtok.c vprintf.c

# only add to this list the modules that you _need_
# some of these are untested!
OBJS =	$(LIB)(strstr.o) \
	$(LIB)(memmove.o) \
	$(LIB)(clock.o) \
#	$(LIB)(vprintf.o) \
#	$(LIB)(fprintf.o) \
#	$(LIB)(sprintf.o) \
#	$(LIB)(strtok.o) \
#	$(LIB)(memchr.o) \
#	$(LIB)(memcmp.o) \
#	$(LIB)(memcpy.o) \
#	$(LIB)(memset.o) \
#	$(LIB)(setvbuf.o) \
#	$(LIB)(strcat.o) \
#	$(LIB)(strchr.o) \
#	$(LIB)(strcmp.o) \
#	$(LIB)(strcpy.o) \
#	$(LIB)(strcspn.o) \
#	$(LIB)(strerror.o) \
#	$(LIB)(strlen.o) \
#	$(LIB)(strncat.o) \
#	$(LIB)(strncmp.o) \
#	$(LIB)(strncpy.o) \
#	$(LIB)(strpbrk.o) \
#	$(LIB)(strrchr.o) \
#	$(LIB)(strspn.o) \
#	$(LIB)(stdio.o) 
#



all:	$(LIB)

link:	$(HDRS) #stdio.h 
	[ -d $(INCL) ] || mkdir $(INCL)
	[ -d $(INCL)/sys ] || mkdir $(INCL)/sys
	($(SHELL) ../mklinks $(INCL) $(HDRS))
	($(SHELL) ../mklinks $(INCL)/sys $(SYSHDRS))
#	($(SHELL) ../mklinks $(INCL) stdio.h)

$(LIB):	$(OBJS)
# if you make doesn't know how to put objects in libraries
# then simply make all the .o's and use the following line
#	ar r $@ $?
	$(RANLIB) $@

stdio.h: stdio.h_std stdio.sed /usr/include/stdio.h 
	sed -f stdio.sed </usr/include/stdio.h >stdio.hacked
	sed <stdio.h_std >stdio.h -e '/%%%/ r stdio.hacked'
	rm stdio.hacked

clean:
	-rm -f *.o *.out stdio.hacked

clobber: clean
	-rm -f $(LIB) stdio.h

string.h: stddef.h

stdlib.h: stddef.h

stdio.h: stddef.h 

time.h: stddef.h

stdio.o: stdio.h

setvbuf.o: stdlib.h stdio.h

fprintf.o: stdarg.h stdio.h

sprintf.o: stdarg.h stdio.h

vprintf.o: stdarg.h stdio.h

strstr.o: string.h 

