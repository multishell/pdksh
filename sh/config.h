/*
 * Configuration file for the PD ksh
 *
 * RCSid: $Id: config.h,v 1.3 1992/05/03 08:28:59 sjg Exp $
 */

#ifndef	_CONFIG_H
#define	_CONFIG_H

/*
 * Builtin edit modes
 */

#define	EMACS				/* EMACS-like mode */
#define	VI				/* vi-like mode */
#define	JOBS				/* job control */

#ifndef SIGINT
#include <signal.h>
#endif

/*
 * leave USE_SIGACT defined.
 * if you don't have sigaction(2) and the
 * implementation in sigact.c doesn't work for your system,
 * fix it.
 * 
 * Of course if your system has a real sigaction() 
 * implementation that is faulty! undef JOBS and add USE_SIGNAL
 * or whatever does work.  You may find it necessary to undef
 * USE_SIGACT, if so please report it.
 */
#define USE_SIGACT			/* POSIX signal handling */
/* 
 * These control how sigact.c implements sigaction()
 * If you don't define any of them it will try and work it out 
 * for itself.  The are listed in order of preference (usefulness).
 */
/* #define USE_SIGMASK			/* BSD4.2 ? signal handling */
/* #define USE_SIGSET			/* BSD4.1 ? signal handling */
/* #define USE_SIGNAL			/* plain old signal(2) */

#if defined(JOBS) && (!defined(SIGCONT) || (defined(_SYSV) && defined(USE_SIGNAL)))
#undef JOBS
#endif

/* #define	FASCIST			/* Fascist getopts */
#define	SHARPBANG			/* Hack to handle #! */
/* #define	SILLY			/* Game of life in EMACS mode */
/* #define	SWTCH			/* Handle SWTCH for shl(1) */

#endif	/* _CONFIG_H */
