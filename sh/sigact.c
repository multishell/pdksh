/* NAME:
 *      sigact.c - fake sigaction(2)
 *
 * SYNOPSIS:
 *      #include <signal.h>
 *      #ifndef  SA_NOCLDSTOP
 *      # include "sigact.h"
 *      #endif
 * 
 *      int sigaction(int sig, struct sigaction *act, 
 *                      struct sigaction *oact);
 *      int sigaddset(sigset_t *mask, int sig);
 *      int sigdelset(sigset_t *mask, int sig);
 *      int sigemptyset(sigset_t *mask);
 *      int sigfillset(sigset_t *mask);
 *      int sigismember(sigset_t *mask, int sig);
 *      int sigpending(sigset_t *set);
 *      int sigprocmask(int how, sigset_t *set, sigset_t *oldset);
 *      int sigsuspend(sigset_t *mask);
 *
 * DESCRIPTION:
 *      This is a fake sigaction implementation.  It uses 
 *      sigset(2) if available, otherwise it just uses 
 *      signal(2).  If it thinks sigaction(2) really exists it 
 *      compiles to almost nothing.
 *      
 *      The need for all this is the problems caused by mixing 
 *      signal handling routines in the one process.  This 
 *      module allows for a consistent POSIX compliant interface 
 *      to whatever is available.
 *
 * RETURN VALUE:
 *      0==success, -1==failure
 *
 * FILES:
 *      None.
 *
 * SEE ALSO:
 *      
 *
 * BUGS:
 *      Since we fake most of this, don't expect fancy usage to 
 *      work.
 *      
 * COPYRIGHT:
 *      @(#)Copyright (c) 1992 Simon J. Gerraty
 *
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code 
 *      is granted subject to the following conditions.
 *      1/ that that the above copyright notice and this notice 
 *      are preserved in all copies and that due credit be given 
 *      to the author.  
 *      2/ that any changes to this code are clearly commented 
 *      as such so that the author does get blamed for bugs 
 *      other than his own.
 *      
 *      Please send copies of changes and bug-fixes to:
 *      sjg@zen.void.oz.au
 *
 */
#ifndef lint
static char  *RCSid = "$Id: sigact.c,v 1.5 1992/05/03 08:29:10 sjg Exp $";
#endif
/*
 * $Log: sigact.c,v $
 * Revision 1.5  1992/05/03  08:29:10  sjg
 * Update for Patch05
 *
 * Revision 1.4  1992/04/29  06:29:13  sjg
 * avoid use of #pragma
 *
 * Revision 1.3  1992/04/26  11:24:43  sjg
 * USE_SIGSET corrected in sigsuspend().
 *
 * Revision 1.2  1992/04/24  15:04:11  sjg
 * now compiles with cc
 *
 * Revision 1.1  1992/04/24  12:03:58  sjg
 * Initial revision
 *
 */

#include <signal.h>

/*
 * some systems have a faulty sigaction() implementation!
 * Allow us to bypass it.
 */
#if !defined(SA_NOCLDSTOP) || defined(USE_SIGNAL) || defined(USE_SIGSET) || defined(USE_SIGMASK)

/*
 * if we haven't been told,
 * try and guess what we should implement with.
 */
#if !defined(USE_SIGSET) && !defined(USE_SIGMASK) && !defined(USE_SIGNAL)
# if defined(sigmask) || defined(BSD) || defined(_BSD) && !defined(BSD41)
#   define USE_SIGMASK
# else
#   ifndef NO_SIGSET
#     define USE_SIGSET
#   else
#     define USE_SIGNAL
#   endif
# endif
#endif
/*
 * if we still don't know, we're in trouble
 */
#if !defined(USE_SIGSET) && !defined(USE_SIGMASK) && !defined(USE_SIGNAL)
error must know what to implement with
#endif

#include "sigact.h"



int
sigaction(sig, act, oact)
  int sig;
  struct sigaction *act, *oact;
{
  void (*oldh)();

  if (act)
  {
#ifdef USE_SIGSET
    oldh = sigset(sig, act->sa_handler);
#else
    oldh = signal(sig, act->sa_handler);
#endif
  }
  else
  {
    if (oact)
    {      
#ifdef USE_SIGSET
      oldh = sigset(sig, SIG_IGN);
#else
      oldh = signal(sig, SIG_IGN);
#endif
      if (oldh != SIG_IGN)
      {
#ifdef USE_SIGSET
	(void) sigset(sig, oldh);
#else
	(void) signal(sig, oldh);
#endif
      }
    }
  }
  if (oact)
  {
    oact->sa_handler = oldh;
  }
  return 0;				/* hey we're faking it */
}

int
sigaddset(mask, sig)
  sigset_t *mask;
  int sig;
{
  *mask |= sigmask(sig);
  return 0;
}


int
sigdelset(mask, sig)
  sigset_t *mask;
  int sig;
{
  *mask &= ~(sigmask(sig));
  return 0;
}


int
sigemptyset(mask)
  sigset_t *mask;
{
  *mask = 0;
  return 0;
}


int
sigfillset(mask)
  sigset_t *mask;
{
  *mask = ~0;
  return 0;
}


int
sigismember(mask, sig)
  sigset_t *mask;
  int sig;
{
  return ((*mask) & sigmask(sig));
}


int
sigpending(set)
  sigset_t *set;
{
  return 0;
}


int
sigprocmask(how, set, oldset)
  int how;
  sigset_t *set, *oldset;
{
#ifdef USE_SIGSET
  register int i;
#endif
  sigset_t sm;

#ifdef USE_SIGMASK
  sm = sigblock(0);
#else
  sm = 0;
#endif
  
  if (oldset)
    *oldset = sm;	/* dangerous ? */
  if (set)
  {
    switch (how)
    {
    case SIG_BLOCK:
      sm |= *set;
      break;
    case SIG_UNBLOCK:
      sm &= ~(*set);
      break;
    case SIG_SETMASK:
      sm = *set;
      break;
    }
#ifdef USE_SIGMASK
    (void) sigsetmask(sm);
#else
# ifdef USE_SIGSET
    for (i = 1; i < NSIG; i++)
    {
      if (how == SIG_UNBLOCK)
      {
	if (*set & sigmask(i))
	  sigrelse(i);
      }
      else
	if (sm & sigmask(i))
	{
	  sighold(i);
	}
    }
# endif
#endif
  }
  return 0;
}


int
sigsuspend(mask)
  sigset_t *mask;
{
#ifdef USE_SIGSET
  int sig = SIGCHLD;			/* our default */
  
  /*
   * add as many tests as you think sensible, but
   * SIGALRM, and SIGCHLD are probably the most
   * common.
   */
  if (*mask & sigmask(SIGALRM))
    sig = SIGALRM;
  else
    if (*mask & sigmask(SIGPIPE))
      sig = SIGPIPE;
  sigpause(sig);
#else
# ifdef USE_SIGMASK
  sigpause(mask);
# else
  pause();
# endif
#endif
  return 0;
}

#endif /* ! SA_NOCLDSTOP */


/* This lot (for GNU-Emacs) goes at the end of the file. */
/* 
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
