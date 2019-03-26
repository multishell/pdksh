/* NAME:
 *      sigact.h - sigaction et al
 *
 * SYNOPSIS:
 *      #include <signal.h>
 *      #ifndef  SA_NOCLDSTOP
 *      # include "sigact.h"
 *      #endif
 *
 * DESCRIPTION:
 *      This header is the interface to a fake sigaction(2) implementation.  
 *      Do NOT include this header unless your system does not 
 *      have a real sigaction(2) implementation.
 */
/*
 * $Log: sigact.h,v $
 * Revision 1.2  1992/04/24  15:04:11  sjg
 * now compiles with cc
 *
 * Revision 1.1  1992/04/24  12:01:38  sjg
 * Initial revision
 *
 */

#ifndef _SIGACT_H
#define _SIGACT_H

#ifndef ARGS
# if defined(__STDC__)
#   define ARGS(p) p
# else
#   define ARGS(p) ()
# endif
#endif
#ifndef __STDC__
# define volatile			/* can change without warning */
# define const				/* read only */
#endif

#ifndef SIGKILL
# include <signal.h>
#endif
#ifndef SA_NOCLDSTOP
/* sa_flags */
#define	SA_NOCLDSTOP	0x0001		/* don't send SIGCHLD on child stop */
#define SA_RESTART	0x0002		/* re-start I/O */

/* sigprocmask flags */
#define	SIG_BLOCK	0x0001
#define	SIG_UNBLOCK	0x0002
#define	SIG_SETMASK	0x0004

#ifndef __sys_stdtypes_h
typedef int sigset_t;
#endif

struct sigaction
{
  void		(*sa_handler)();
  sigset_t	sa_mask;
  int		sa_flags;
};


int	sigaction	ARGS(( int sig, struct sigaction *act, struct sigaction *oact ));
int	sigaddset	ARGS(( sigset_t *mask, int sig ));
int	sigdelset	ARGS(( sigset_t *mask, int sig ));
int	sigemptyset	ARGS(( sigset_t *mask ));
int	sigfillset	ARGS(( sigset_t *mask ));
int	sigismember	ARGS(( sigset_t *mask, int sig ));
int	sigpending	ARGS(( sigset_t *set ));
int	sigprocmask	ARGS(( int how, sigset_t *set, sigset_t *oldset ));
int	sigsuspend	ARGS(( sigset_t *mask ));

#ifndef sigmask
#define sigmask(m)	(1<<((m)-1))	/* convert SIGnum to mask */
#endif
#if !defined(NSIG) && defined(_NSIG)
# define NSIG _NSIG
#endif
#endif /* ! SA_NOCLDSTOP */
#endif /* _SIGACT_H */
/*
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
