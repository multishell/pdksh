#
#  @configure_input@
#

srcdir = .
VPATH = .

CC = gcc
CPP = cpp

INSTALL = /bin/install -c
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

DEFS = -DHAVE_CONFIG_H
LIBS = -los2

CPPFLAGS = 
CFLAGS = -O -DOS2
LDSTATIC = 
LDFLAGS = -O $(LDSTATIC)

prefix = c:/usr
exec_prefix = c:/usr
binprefix =
manprefix =

bindir = $(exec_prefix)/bin
mandir = $(prefix)/man/man$(manext)
manext = 1

# Suffix for executables: nothing for unix, .exe for os/2.
exe_suffix=.exe

SHELL = /bin/sh

SRCS = alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c \
	eval.c exec.c expr.c history.c io.c jobs.c lex.c mail.c \
	main.c misc.c missing.c path.c shf.c sigact.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c vi.c
OBJS = os2.o alloc.o c_ksh.o c_sh.o c_test.o c_ulimit.o edit.o emacs.o \
	eval.o exec.o expr.o history.o io.o jobs.o lex.o mail.o \
	main.o misc.o missing.o path.o shf.o sigact.o syn.o table.o trap.o \
	tree.o tty.o var.o version.o vi.o
HDRS = c_test.h edit.h expand.h ksh_dir.h ksh_limval.h ksh_stat.h ksh_time.h \
	ksh_times.h ksh_wait.h lex.h options.h proto.h sh.h shf.h sigact.h \
	table.h tree.h tty.h
DISTFILES = $(SRCS) $(HDRS) ksh.1 Makefile.in configure.in \
	config.h.in conf-end.h acconfig.h aclocal.m4 emacs-gen.sh \
	mkinstalldirs install-sh new-version.sh siglist.in siglist.sh \
	check-fd.c check-pgrp.c check-sigs.c \
	README NEWS CONTRIBUTORS PROJECTS INSTALL NOTES BUG-REPORTS \
	Bugs ChangeLog configure stamp-h.in
# ETCFILES also disted, but handled differently
ETCFILES = etc/ksh.kshrc etc/profile etc/sys_config.sh
# MISCFILES also disted, but handled differently
MISCFILES = misc/COPYING misc/ChangeLog.sjg misc/Changes.jrm misc/Changes.mlj \
	misc/Changes.pc misc/README.sjg misc/ReadMe.eg misc/ReadMe.emacs \
	misc/ReadMe.jrm
# OS2FILES also disted, but handled differently
OS2FILES = os2/Makefile os2/config.h os2/config.status os2/configure.cmd \
	os2/emacs.out os2/kshrc.ksh os2/make.sed os2/os2.c os2/os2siglist.out \
	os2/README.os2 os2/os2bugs

.PRECIOUS: configure config.h.in Makefile config.status

all: ksh$(exe_suffix)

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) -I. -I$(srcdir) $(CFLAGS) $<

install: installdirs all $(srcdir)/ksh.1
	$(INSTALL_PROGRAM) ksh$(exe_suffix) $(bindir)/$(binprefix)ksh$(exe_suffix)
	-$(INSTALL_DATA) $(srcdir)/ksh.1 $(mandir)/$(manprefix)ksh.$(manext)
	-@test -f /etc/shells \
	    && (grep '^$(bindir)/$(binprefix)ksh$$' /etc/shells > /dev/null \
		|| echo \
'NOTE: /etc/shells does not contain $(bindir)/$(binprefix)ksh \
      you should add it if you want to set your shell to ksh')

installdirs:
	$(srcdir)/mkinstalldirs $(bindir) $(mandir)

uninstall:
	rm -f $(bindir)/$(binprefix)ksh$(exe_suffix)
	rm -f $(mandir)/$(manprefix)ksh.$(manext)

check:
	ENV= ./ksh $(srcdir)/Bugs ./ksh

ksh$(exe_suffix): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

info:
	@echo "No info (yet)"
dvi:

$(srcdir)/configure: configure.in aclocal.m4
	cd $(srcdir) && autoconf

$(srcdir)/config.h.in: stamp-h.in
$(srcdir)/stamp-h.in: configure.in aclocal.m4 acconfig.h
	cd $(srcdir) && autoheader
	date > $(srcdir)/stamp-h.in

config.h:
	cmd /c copy $(srcdir)\os2\config.h config.h > nul
	touch config.h

Makefile:
	cmd /c copy $(srcdir)\os2\Makefile Makefile > nul
	touch Makefile

# two steps to prevent the creation of a bogus siglist.out
siglist.out:
	cmd /c copy $(srcdir)\os2\os2siglist.out siglist.out > nul
	touch siglist.out

# two steps to prevent the creation of a bogus emacs.out
emacs.out:
	cmd /c copy $(srcdir)\os2\emacs.out emacs.out > nul
	touch emacs.out

debugtools: check-fd$(exe_suffix) check-sigs$(exe_suffix) \
	check-pgrp$(exe_suffix)

check-fd.o check-pgrp.o check-sigs.o: config.h
check-fd$(exe_suffix): check-fd.o
	$(CC) $(LDFLAGS) -o $@ check-fd.o $(LIBS)

check-pgrp$(exe_suffix): check-pgrp.o
	$(CC) $(LDFLAGS) -o $@ check-pgrp.o $(LIBS)

check-sigs$(exe_suffix): check-sigs.o
	$(CC) $(LDFLAGS) -o $@ check-sigs.o $(LIBS)

TAGS: $(SRCS) $(HDRS)
	cd $(srcdir) && etags $(SRCS) $(HDRS)

tags: $(SRCS) $(HDRS)
	cd $(srcdir) && ctags -wt $(SRCS) $(HDRS)

clean:
	rm -f ksh$(exe_suffix) $(OBJS) siglist.out emacs.out core \
	      version.c.bak Makefile.bak Makefile.tmp check-fd$(exe_suffix) \
	      check-pgrp$(exe_suffix) check-sigs$(exe_suffix)

mostlyclean: clean

distclean: clean
	rm -f Makefile config.h stamp-h config.status config.log config.cache \
	      tags TAGS *~

realclean: distclean

dist: $(DISTFILES) $(ETCFILES) $(MISCFILES) $(OS2FILES)
	cd $(srcdir) && \
	  { \
	    ./new-version.sh; \
	    FNAME=pdksh-`sed -e '/"@(.)/!d' \
		    -e 's/[^0-9]*\([0-9.]*\).*/\1/' -e q version.c`; \
	    echo Creating version $$FNAME; \
	    rm -rf $$FNAME; \
	    mkdir $$FNAME $$FNAME/etc $$FNAME/misc $$FNAME/os2; \
	    cp -p $(DISTFILES) $$FNAME; \
	    cp -p $(ETCFILES) $$FNAME/etc; \
	    cp -p $(MISCFILES) $$FNAME/misc; \
	    cp -p $(OS2FILES) $$FNAME/os2; \
	    ./emacs-gen.sh emacs.c > os2/emacs.out; \
	    test -x ./Dist-fixup && ./Dist-fixup $$FNAME; \
	    sed -f os2/make.sed < $$FNAME/Makefile.in > $$FNAME/os2/Makefile; \
	    chmod -R a+rX,u+w,og-w $$FNAME; \
	    tar chzf $$FNAME.tar.gz $$FNAME; \
	    find $$FNAME -print | xargs pathchk -p; \
	  }

depend: $(SRCS)
	sed -n '1,/[ ]PUT ANYTHING BELOW THIS LINE/p' < Makefile > Makefile.tmp
	srcs=; for i in $(SRCS) ; do srcs="$$srcs $(srcdir)/$$i"; done; \
	  $(CC) -M $(DEFS) -I. -I$(srcdir) $(CFLAGS) $$srcs | \
	    sed -e 's?[ 	]/[^ 	]*??g' -e 's?[ 	]./? ?g' \
		-e 's?[ 	]$(srcdir)//*? ?g' -e 's?^$(srcdir)//*??' \
		-e '/^[ 	]*\\[	 ]*$$/d' -e '/^[^:]*:[	 ]*$$/d' \
		-e 's/^\([	 ]*\)$$/\1sh.h/' \
	    >> Makefile.tmp
	mv Makefile.tmp Makefile
	@echo 'Make depend done (stopping make)'; false

# DON'T PUT ANYTHING BELOW THIS LINE (and don't delete it - its for make depend)
alloc.o : alloc.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
c_ksh.o : c_ksh.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h 
c_sh.o : c_sh.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h ksh_time.h \
  ksh_times.h 
c_test.o : c_test.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h c_test.h 
c_ulimit.o : c_ulimit.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_time.h \
  sh.h
edit.o : edit.c config.h options.h conf-end.h sh.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h tty.h \
  edit.h 
emacs.o : emacs.c config.h options.h conf-end.h sh.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h ksh_dir.h \
  edit.h emacs.out 
eval.o : eval.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h \
  ksh_dir.h \
  ksh_stat.h 
exec.o : exec.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h c_test.h \
  ksh_stat.h 
expr.o : expr.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
history.o : history.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h 
io.o : io.c sh.h \
  config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h 
jobs.o : jobs.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_wait.h \
  ksh_times.h tty.h \
  sh.h
lex.o : lex.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
mail.o : mail.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h ksh_time.h \
  sh.h
main.o : main.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h ksh_time.h \
  sh.h
misc.o : misc.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h \
  sh.h
missing.o : missing.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h ksh_dir.h \
  ksh_time.h \
  ksh_times.h 
path.o : path.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_stat.h 
shf.o : shf.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_limval.h 
sigact.o : sigact.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
syn.o : syn.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h c_test.h 
table.o : table.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
trap.o : trap.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h siglist.out 
tree.o : tree.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h 
tty.o : tty.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h tty.h \
  sh.h
var.o : var.c sh.h config.h options.h conf-end.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h ksh_time.h \
  ksh_limval.h \
  ksh_stat.h 
version.o : version.c 
vi.o : vi.c config.h options.h conf-end.h sh.h \
  shf.h table.h \
  tree.h expand.h lex.h proto.h \
  ksh_stat.h edit.h 
