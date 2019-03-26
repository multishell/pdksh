dnl Copyright (C) 1994, Memorial University of Newfoundland.
dnl This file is covered by the GNU General Public License, version 2, see
dnl the file misc/COPYING for details.
dnl
dnl
dnl Ksh specific tests
dnl
dnl
dnl Check if dup2() does not clear the close on exec flag
define(KSH_DUP2_CLEXEC_CHECK,
[AC_PROVIDE([$0])AC_CHECKING(if dup2() fails to reset the close-on-exec flag)
  AC_TEST_PROGRAM([
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#ifndef F_GETFD
# define F_GETFD 1
#endif
#ifndef F_SETFD
# define F_SETFD 2
#endif
#ifndef O_RDONLY
# define O_RDONLY 0
#endif

/* On some systems (Ultrix 2.1..4.2 (and more?)), dup2() does not clear
   the close on exec flag */
main()
{
  int fd1, fd2;
  fd1 = open("/dev/null", O_RDONLY);
  if (fcntl(fd1, F_SETFD, 1) < 0)
    exit(1);
  fd2 = dup2(fd1, fd1 + 1);
  if (fd2 < 0)
    exit(2);
  exit(fcntl(fd2, F_GETFD, 0) == 0 ? 0 : 3);
}
  ], , AC_DEFINE(DUP2_BROKEN))])dnl
dnl
dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
define(KSH_SIGNAL_CHECK,
[AC_REQUIRE([AC_RETSIGTYPE])AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(posix signals, [#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], AC_DEFINE(POSIX_SIGNALS), AC_DEFINE(USE_FAKE_SIGACT) dnl
  AC_COMPILE_CHECK(BSD4.2 signals, [#include <signal.h>], [
    int mask = sigmask(SIGINT);
    sigsetmask(mask); sigblock(mask); sigpause(mask);
  ], AC_DEFINE(BSD42_SIGNALS),
  AC_COMPILE_CHECK(BSD4.1 signals, [
    #include <signal.h>
    RETSIGTYPE foo() { }
  ], [
    int mask = sigmask(SIGINT);
    sigset(SIGINT, foo); sigrelse(SIGINT); sighold(SIGINT); sigpause(SIGINT);
  ], AC_DEFINE(BSD41_SIGNALS), AC_DEFINE(V7_SIGNALS))))])dnl
dnl
dnl What kind of process groups: POSIX, BSD, or SYSV
dnl	BSD uses setpgrp(pid, pgrp), getpgrp(pid)
dnl	POSIX uses setpid(pid, pgrp), getpgrp(void)
dnl	SYSV uses setpgrp(void), getpgrp(void)
dnl Checks for BSD first since the posix test may succeed on BSDish systems
dnl (depends on what random value gets passed to getpgrp()).
define(KSH_PGRP_CHECK,
[AC_REQUIRE([KSH_UNISTD_H])AC_PROVIDE([$0])dnl
  AC_CHECKING(for POSIX vs BSD vs SYSV setpgrp()/getpgrp())
  AC_TEST_PROGRAM([
#include <signal.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

    main()
    {
      int ecode = 0, child = fork();
      if (child < 0)
	exit(1);
      if (child == 0) {
	signal(SIGTERM, SIG_DFL); /* just to make sure */
	sleep(10);
	exit(9);
      }
      if (setpgrp(child, child) < 0)
	ecode = 2;
      else if (getpgrp(child) != child)
	ecode = 3;
      kill(child, SIGTERM);
      exit(ecode);
    }
  ], AC_DEFINE(BSD_PGRP),
    AC_TEST_PROGRAM([[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

      main()
      {
	int child;
	int n, p1[2], p2[2];
	char buf[1];

	if (pipe(p1) < 0 || pipe(p2) < 0)
	  exit(1);
	if ((child = fork()) < 0)
	  exit(2);
	if (child == 0) {
	  n = read(p1[0], buf, sizeof(buf)); /* wait for parent to setpgid */
	  buf[0] = (n != 1 ? 10 : (getpgrp() != getpid() ? 11 : 0));
	  if (write(p2[1], buf, sizeof(buf)) != 1)
	    exit(12);
	  exit(0);
	}
	if (setpgid(child, child) < 0)
	  exit(3);
	if (write(p1[1], buf, 1) != 1)
	  exit(4);
	if (read(p2[0], buf, 1) != 1)
	  exit(5);
	exit((int) buf[0]);
      }
    ]], AC_DEFINE(POSIX_PGRP),
      AC_TEST_PROGRAM([[[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

	  main()
	  {
	    int child;
	    int n, p[2];
	    char buf[1];

	    if (pipe(p) < 0)
	      exit(1);
	    if ((child = fork()) < 0)
	      exit(2);
	    if (child == 0) {
	      buf[0] = (setpgrp() < 0 ? 10 : (getpgrp() != getpid() ? 11 : 0));
	      if (write(p[1], buf, sizeof(buf)) != 1)
		exit(11);
	      exit(0);
	    }
	    if (read(p[0], buf, 1) != 1)
	      exit(3);
	    exit((int) buf[0]);
	  }
        ]]], AC_DEFINE(SYSV_PGRP))))])dnl
dnl
dnl
dnl Check if the pgrp of setpgrp() can't be the pid of a zombie process.
dnl On some systems, the kernel doesn't count zombie processes when checking
dnl if a process group is valid, which can cause problems in creating the
dnl pipeline "cmd1 | cmd2": if cmd1 can die (and go into the zombie state)
dnl before cmd2 is started, the kernel doesn't allow the setpgrp() for cmd2
dnl to succeed.  This test defines NEED_PGRP_SYNC if the kernel has this bug.
define(KSH_PGRP_SYNC,
[AC_PROVIDE([$0])AC_REQUIRE([KSH_PGRP_CHECK])dnl
 AC_CHECKING(if process group synchronization is required)
  AC_TEST_PROGRAM([
      main()
      {
#if defined(POSIX_PGRP) || defined(BSD_PGRP)
# ifdef POSIX_PGRP
#  define getpgID()	getpgrp()
# else
#  define getpgID()	getpgrp(0)
#  define setpgid(x,y)	setpgrp(x,y)
# endif
	int pid1, pid2, fds[2];
	int status;
	char ok;

	switch (pid1 = fork()) {
	  case -1:
	    exit(1);
	  case 0:
	    setpgid(0, getpid());
	    exit(0);
	}
	setpgid(pid1, pid1);

	sleep(2);	/* let first child die */

	if (pipe(fds) < 0)
	  exit(2);

	switch (pid2 = fork()) {
	  case -1:
	    exit(3);
	  case 0:
	    setpgid(0, pid1);
	    ok = getpgID() == pid1;
	    write(fds[1], &ok, 1);
	    exit(0);
	}
	setpgid(pid2, pid1);

	close(fds[1]);
	if (read(fds[0], &ok, 1) != 1)
	  exit(4);
	wait(&status);
	wait(&status);
	exit(ok ? 0 : 5);
#else /* POSIX_PGRP || BSD_PGRP */
	exit(20);
#endif /* POSIX_PGRP || BSD_PGRP */
      }
    ],,AC_DEFINE(NEED_PGRP_SYNC))])dnl
dnl
dnl
dnl
dnl Somewhat generic tests that aren't in autoconf, but perhaps should be
dnl
dnl Like AC_HAVE_HEADER(unistd.h) but only defines HAVE_UNISTD_H if
dnl the header file is sane (MIPS RISC/os 5.0 (and later?) bas a unistd.h
dnl in the bsd43 environ that is incorrect - it defines POSIX_VERSION even
dnl though its non-posix).
define(KSH_UNISTD_H,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(sane unistd.h, [
#include <unistd.h>
#ifdef _POSIX_VERSION
# include <dirent.h>
#endif
    ], , AC_DEFINE(HAVE_UNISTD_H))])dnl
dnl
dnl Some systems (eg, SunOS 4.0.3) have <termios.h> and <termio.h> but don't
dnl have the related functions/defines (eg, tcsetattr(), TCSADRAIN, etc.)
dnl or the functions don't work well with tty process groups.  Sun's bad
dnl termios can be detected by the lack of tcsetattr(), but its bad termio
dnl is harder to detect - so check for (sane) termios first, then check for
dnl BSD, then termio.
define(KSH_TERM_CHECK,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(sane termios.h, [#include <termios.h>],
    [struct termios t;
     tcgetattr(0, &t); tcsetattr(0, TCSADRAIN, &t);
     ],
    AC_DEFINE(HAVE_TERMIOS_H),
      AC_COMPILE_CHECK(BSD tty interface, [#include <sys/ioctl.h>],
	[
	  struct sgttyb sb; ioctl(0, TIOCGETP, &sb);
#ifdef TIOCGATC
	  { struct ttychars lc; ioctl(0, TIOCGATC, &lc); }
#else /* TIOCGATC */
	  { struct tchars tc; ioctl(0, TIOCGETC, &tc); }
# ifdef TIOCGLTC
	  { struct ltchars ltc; ioctl(0, TIOCGLTC, &ltc); }
# endif /* TIOCGLTC */
#endif /* TIOCGATC */
	], , [[AC_HAVE_HEADERS(termio.h)]]))])dnl
dnl
define(KSH_VOID,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(lack of working void, [
      void foo() { }
      /* Some compilers (old pcc ones) like "void *a;", but a can't be used */
      void *bar(a) void *a; { int *b = (int *) a; *b = 1; }
    ], , , AC_DEFINE(void,char))])dnl
dnl
dnl Early MIPS compilers (used in Ultrix 4.2) don't like
dnl "int x; int *volatile a = &x; *a = 0;"
define(KSH_VOLATILE,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(lack of working volatile, [int x, y, z;],
    [volatile int a; int * volatile b = x ? &y : &z;
    /* Older MIPS compilers (eg., in Ultrix 4.2) don't like *b = 0 */
    *b = 0;], ,
    AC_DEFINE(volatile,))])dnl
dnl
dnl Check if function prototypes work (including stdc vararg prototypes)
define(KSH_PROTOTYPES,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(working function prototypes, [
#include <stdarg.h>
void foo(char *fmt, ...);
int bar(int a, char b, char *c);
int bar(a, b, c) int a; char b; char *c;
{ foo("%d%c%s\n", a, b, c); return a + b + *c; }
void foo(char *fmt, ...) { va_list a; va_start(a, fmt); va_end(a); }
    ], , AC_DEFINE(HAVE_PROTOTYPES))])dnl
dnl
dnl Figure out what integer type has 32 bits (assuming 8 bit bytes)
define(KSH_INT32,
[AC_PROVIDE([$0])AC_CHECKING(for signed 32 bit integer type)
  AC_TEST_PROGRAM([main() { if (sizeof(long) != 4) exit(1); exit(0); }],
    AC_DEFINE(INT32, long), AC_DEFINE(INT32, int))])dnl
dnl
define(KSH_CLOCK_T,
[AC_REQUIRE([KSH_INT32])AC_PROVIDE([$0])AC_CHECKING(for clock_t in sys/types.h)
  AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])clock_t([^a-zA-Z0-9_]|\$)]], sys/types.h, ,
    AC_DEFINE(clock_t, INT32))])dnl
dnl
define(KSH_TIME_T,
[AC_REQUIRE([KSH_INT32])AC_PROVIDE([$0])AC_CHECKING(for time_t in sys/types.h)
  AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])time_t([^a-zA-Z0-9_]|\$)]], sys/types.h, ,
    AC_DEFINE(time_t, INT32))])dnl
dnl
define(KSH_SIGSET_T,
[AC_PROVIDE([$0])AC_CHECKING(for sigset_t in signal.h)
  AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])sigset_t([^a-zA-Z0-9_]|\$)]], signal.h, ,
    AC_DEFINE(sigset_t, unsigned))])dnl
dnl
define(KSH_TIME_DECLARED,
[AC_REQUIRE([KSH_TIME_T])AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(time() declaration in time.h, [#include <time.h>],
    [time_t (*f)() = time;], AC_DEFINE(TIME_DECLARED))])dnl
dnl
dnl Check for sys_errlist[] and sys_nerr, check for declaration
define(KSH_SYS_ERRLIST,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(sys_errlist declaration in errno.h,
    [#include <errno.h>], [char *msg = *(sys_errlist + sys_nerr - 1);],
    AC_DEFINE(HAVE_SYS_ERRLIST) AC_DEFINE(SYS_ERRLIST_DECLARED),
    AC_COMPILE_CHECK(sys_errlist, [#include <errno.h>], [[
	extern char *sys_errlist[];
	extern int sys_nerr;
	char *p;
	p = sys_errlist[sys_nerr - 1];
      ]], AC_DEFINE(HAVE_SYS_ERRLIST)))])dnl
dnl
dnl Check for sys_siglist[], check for declaration
define(KSH_SYS_SIGLIST,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(sys_siglist declaration in signal.h or unistd.h,
    [
#include <signal.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
      ], [char *msg = sys_siglist[1];],
    AC_DEFINE(HAVE_SYS_SIGLIST) AC_DEFINE(SYS_SIGLIST_DECLARED),
    AC_COMPILE_CHECK(sys_siglist, [
#include <signal.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
	  ], [[
	extern char *sys_siglist[];
	char *p = sys_siglist[2];
      ]], AC_DEFINE(HAVE_SYS_SIGLIST)))])dnl
dnl
dnl Check if C compiler understands gcc's __attribute((...)).
dnl checks for noreturn, const, and format(type,fmt,param), also checks
dnl that the compiler doesn't die when it sees an unknown attribute (this
dnl isn't perfect since gcc doesn't parse unknown attributes with parameters)
define(KSH_GCC_FUNC_ATTR,
[AC_PROVIDE([$0])AC_CHECKING(if C compiler groks __attribute__(( .. )))
  AC_COMPILE_CHECK(, [
#include <stdarg.h>
void test_fmt(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void test_fmt(char *fmt, ...) { return; }
void test_cnst(int) __attribute__((const));
void test_cnst(int x) { return x + 1; }
void test_nr() __attribute__((noreturn));
void test_nr() { exit(1); }
void test_uk() __attribute__((blah));
void test_uk() { return; }
], [test_nr("%d", 10); test_cnst(2); test_uk(); test_nr();],
    AC_DEFINE(GCC_FUNC_ATTR))])dnl
dnl
dnl
dnl Check for working times (ie, it exists and doesn't always return 0).
dnl Defines TIMES_BROKEN if it doesn't exist or if it always returns 0
dnl (also checks for existance of getrusage if times doesn't work).
define(KSH_TIMES_CHECK,
[AC_PROVIDE([$0])AC_REQUIRE([KSH_CLOCK_T])dnl
 AC_CHECKING(for broken/missing times())
  AC_TEST_PROGRAM([
#include <sys/types.h>
#include <sys/times.h>

      main()
      {
	extern clock_t times();
	struct tms tms;
	times(&tms);
	sleep(1);
	if (times(&tms) == 0)
	  exit(1);
	exit(0);
      }
    ],,AC_DEFINE(TIMES_BROKEN)[AC_HAVE_FUNCS(getrusage)])])dnl
dnl
dnl
dnl Check to see if opendir will open non-directories (not a nice thing)
define(KSH_OPENDIR_CHECK,
[AC_PROVIDE([$0])AC_REQUIRE([AC_DIR_HEADER])dnl
 AC_CHECKING(if opendir() opens non-directories)
  AC_TEST_PROGRAM([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(DIRENT) || defined(_POSIX_VERSION)
# include <dirent.h>
#else
# define dirent direct
# ifdef SYSNDIR
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef SYSDIR
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef NDIR
#  include <ndir.h>
# endif /* NDIR */
#endif /* DIRENT || _POSIX_VERSION */

      main()
      {
	int i, ret = 0;
	FILE *fp;
	char *fname = "conftestod", buf[256];

	for (i = 0; i < sizeof(buf); i++) /* memset(buf, 0, sizeof(buf)) */
	  buf[i] = 0;
	unlink(fname); /* paranoia */
	i = ((fp = fopen(fname, "w")) == (FILE *) 0 && (ret = 1))
	     || (fwrite(buf, sizeof(buf), 1, fp) != 1 && (ret = 2))
	     || (fclose(fp) == EOF && (ret = 3))
	     || (opendir(fname) && (ret = 4))
	     || (opendir("/dev/null") && (ret = 5));

	unlink(fname);
	exit(ret);
      }
    ],,AC_DEFINE(OPENDIR_DOES_NONDIR))])dnl
dnl
dnl
dnl Modified tests from acspecific.m4: need to be more careful when using
dnl AC_HEADER_EGREP - some systems (eg, MIPS RISC/os 5.0, bsd environ) define
dnl o_mode_t, o_pid_t, etc. but do not define mode_t, pid_t, etc.
dnl
undefine([AC_UID_T])undefine([AC_PROVIDE_AC_UID_T])dnl
define(AC_UID_T,
[AC_PROVIDE([$0])AC_CHECKING(for uid_t in sys/types.h)
AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])uid_t([^a-zA-Z0-9_]|\$)]], sys/types.h, ,
  AC_DEFINE(uid_t, int) AC_DEFINE(gid_t, int))])dnl
dnl
undefine([AC_SIZE_T])undefine([AC_PROVIDE_AC_SIZE_T])dnl
define(AC_SIZE_T,
[AC_CHECKING(for size_t in sys/types.h)
AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])size_t([^a-zA-Z0-9_]|\$)]], sys/types.h, , AC_DEFINE(size_t, unsigned))])dnl
dnl
undefine([AC_PID_T])undefine([AC_PROVIDE_AC_PID_T])dnl
define(AC_PID_T,
[AC_PROVIDE([$0])AC_CHECKING(for pid_t in sys/types.h)
AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])pid_t([^a-zA-Z0-9_]|\$)]], sys/types.h, , AC_DEFINE(pid_t, int))])dnl
dnl
undefine([AC_OFF_T])undefine([AC_PROVIDE_AC_OFF_T])dnl
define(AC_OFF_T,
[AC_PROVIDE([$0])AC_CHECKING(for off_t in sys/types.h)
AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])off_t([^a-zA-Z0-9_]|\$)]], sys/types.h, , AC_DEFINE(off_t, long))])dnl
dnl
undefine([AC_MODE_T])undefine([AC_PROVIDE_AC_MODE_T])dnl
define(AC_MODE_T,
[AC_CHECKING(for mode_t in sys/types.h)
AC_HEADER_EGREP([[(^|[^a-zA-Z0-9_])mode_t([^a-zA-Z0-9_]|\$)]], sys/types.h, , AC_DEFINE(mode_t, int))])dnl
dnl
dnl
dnl
dnl Modified test from acspecific.m4: MMAP test needs to check for/use
dnl the MAP_FILE flag. (Needed for older NetBSD systems).
dnl
undefine([AC_MMAP])define(AC_MMAP, [
AC_CHECKING(for working mmap)
AC_TEST_PROGRAM([/* Thanks to Mike Haertel and Jim Avera for this test. */
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

#ifdef BSD
#ifndef BSD4_1
#define HAVE_GETPAGESIZE
#endif
#endif
#ifndef HAVE_GETPAGESIZE
#include <sys/param.h>
#ifdef EXEC_PAGESIZE
#define getpagesize() EXEC_PAGESIZE
#else
#ifdef NBPG
#define getpagesize() NBPG * CLSIZE
#ifndef CLSIZE
#define CLSIZE 1
#endif /* no CLSIZE */
#else /* no NBPG */
#ifdef NBPC
#define getpagesize() NBPC
#else /* no NBPC */
#define getpagesize() PAGESIZE /* SVR4 */
#endif /* no NBPC */
#endif /* no NBPG */
#endif /* no EXEC_PAGESIZE */
#endif /* not HAVE_GETPAGESIZE */

#ifdef __osf__
#define valloc malloc
#endif

#ifndef MAP_FILE
# define MAP_FILE 0
#endif /* MAP_FILE */

extern char *valloc();
extern char *malloc();

int
main()
{
  char *buf1, *buf2, *buf3;
  int i = getpagesize(), j;
  int i2 = getpagesize()*2;
  int fd;

  buf1 = valloc(i2);
  buf2 = valloc(i);
  buf3 = malloc(i2);
  for (j = 0; j < i2; ++j)
    *(buf1 + j) = rand();
  fd = open("conftestmmap", O_CREAT | O_RDWR, 0666);
  write(fd, buf1, i2);
  mmap(buf2, i, PROT_READ | PROT_WRITE, MAP_FILE | MAP_FIXED | MAP_PRIVATE, fd, 0);
  for (j = 0; j < i; ++j)
    if (*(buf1 + j) != *(buf2 + j))
      exit(1);
  lseek(fd, (long)i, 0);
  read(fd, buf2, i); /* read into mapped memory -- file should not change */
  /* (it does in i386 SVR4.0 - Jim Avera) */
  lseek(fd, (long)0, 0);
  read(fd, buf3, i2);
  for (j = 0; j < i2; ++j)
    if (*(buf1 + j) != *(buf3 + j))
      exit(1);
  exit(0);
}
], AC_DEFINE(HAVE_MMAP))
])dnl
dnl
dnl
dnl
dnl Nabbed from gnu make-3.70, wrapped in AC_UNION_WAIT macro.  Requires
dnl AC_HAVE_FUNCS(waitpid), but can't specify this easily.
dnl Check out the wait reality.
define(AC_UNION_WAIT,
[AC_PROVIDE([$0])dnl
  AC_COMPILE_CHECK(union wait, [#include <sys/types.h>
#include <sys/wait.h>],
    [union wait status; int pid; pid = wait (&status);
#ifdef WEXITSTATUS
      /* Some POSIXoid systems have both the new-style macros and the old
	 union wait type, and they do not work together.  If union wait
	 conflicts with WEXITSTATUS et al, we don't want to use it at all.  */
      if (WEXITSTATUS (status) != 0) pid = -1;
#endif
#ifdef HAVE_WAITPID
      /* Make sure union wait works with waitpid.  */
      pid = waitpid (-1, &status, 0);
#endif
    ], AC_DEFINE(HAVE_UNION_WAIT))])dnl
