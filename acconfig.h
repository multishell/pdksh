/* Copyright (C) 1994, Memorial University of Newfoundland.
 * This file is covered by the GNU General Public License, version 2, see
 * the file misc/COPYING for details.
 */
/* Define if your kernal doesn't handle scripts starting with #! */
#undef SHARPBANG

/* Define if dup2() preserves the close-on-exec flag (ultrix does this) */
#undef DUP2_BROKEN

/* Define if you have posix signal routines (sigaction(), et. al.) */
#undef POSIX_SIGNALS

/* Define if you have BSD4.2 signal routines (sigsetmask(), et. al.) */
#undef BSD42_SIGNALS

/* Define if you have BSD4.1 signal routines (sigset(), et. al.) */
#undef BSD41_SIGNALS

/* Define if you have v7 signal routines (signal(), signal reset on delivery) */
#undef V7_SIGNALS

/* Define to use the fake posix signal routines (sigact.[ch]) */
#undef USE_FAKE_SIGACT

/* Define if you have bsd versions of the setpgrp() and getpgrp() routines */
#undef BSD_PGRP

/* Define if you have bsd versions of the setpgid() and getpgrp() routines */
#undef POSIX_PGRP

/* Define if you have sysV versions of the setpgrp() and getpgrp() routines */
#undef SYSV_PGRP

/* Define to char if your compiler doesn't like the void keyword */
#undef void

/* Define to nothing if compiler doesn't like the volatile keyword */
#undef volatile

/* Define to 32-bit signed integer type */
#undef INT32

/* Define to 32-bit signed integer type if <sys/types.h> doesn't define */
#undef clock_t

/* Define to 32-bit signed integer type if <sys/types.h> doesn't define */
#undef time_t

/* Define if time() is declared in <time.h> */
#undef TIME_DECLARED

/* Define to `unsigned' if <signal.h> doesn't define */
#undef sigset_t

/* Define if sys_errlist[] and sys_nerr are in the C library */
#undef HAVE_SYS_ERRLIST

/* Define if sys_errlist[] and sys_nerr are defined in <errno.h> */
#undef SYS_ERRLIST_DECLARED

/* Define if sys_siglist[] is in the C library */
#undef HAVE_SYS_SIGLIST

/* Define if sys_siglist[] are defined in <signal.h> or <unistd.h> */
#undef SYS_SIGLIST_DECLARED

/* Define if C compiler groks function prototypes */
#undef HAVE_PROTOTYPES

/* Define if C compiler groks __attribute__((...)) (const, noreturn, format) */
#undef GCC_FUNC_ATTR

/* Define if you have a sane <unistd.h> header file */
#undef HAVE_UNISTD_H

/* Define if you have a sane <termios.h> header file */
#undef HAVE_TERMIOS_H

/* Define if you don't have times() or if it always returns 0 */
#undef TIMES_BROKEN

/* Define if opendir() will open non-directory files */
#undef OPENDIR_DOES_NONDIR

/* Define if the pgrp of setpgrp() can't be the pid of a zombie process */
#undef NEED_PGRP_SYNC
