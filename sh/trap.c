/*
 * signal handling
 */

#ifndef lint
static char *RCSid = "$Id: trap.c,v 1.2 1992/04/24 12:01:38 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include "sh.h"

Trap sigtraps [SIGNALS] = {
	{0,	"EXIT", "Signal 0"}, /* todo: belongs in e.loc->exit */
	{SIGHUP, "HUP", "Hangup"},
	{SIGINT, "INT", "Interrupt"},
	{SIGQUIT, "QUIT", "Quit"},
	{SIGILL, "ILL", "Illegal instruction"},
	{SIGTRAP, "TRAP", "Trace trap"},
#ifdef	SIGABRT
	{SIGIOT, "ABRT", "Abort"},
#else
	{SIGIOT, "IOT", "IOT instruction"},
#endif
	//{SIGEMT, "EMT", "EMT trap"},
    {SIGBUS, "BUS", "BUS trap"},
	{SIGFPE, "FPE", "Floating exception"},
	{SIGKILL, "KILL", "Killed"},
#ifdef _MINIX
	{SIGUSR1, "USR1", "User defined signal 1"},
	{SIGSEGV, "SEGV", "Memory fault"},
	{SIGUSR2, "USR2", "User defined signal 2"},
#else
	{SIGBUS, "BUS", "Bus error"},
	{SIGSEGV, "SEGV", "Memory fault"},
	{SIGSYS, "SYS", "Bad system call"},
#endif
	{SIGPIPE, "PIPE", "Broken pipe"},
	{SIGALRM, "ALRM", "Alarm clock"},
	{SIGTERM, "TERM", "Terminated"},
#ifdef _MINIX
	{SIGSTKFLT, "STKFLT", "Stack fault"},
#endif
#ifdef _SYSV
	{SIGUSR1, "USR1", "User defined signal 1"},
	{SIGUSR2, "USR2", "User defined signal 2"},
	{SIGCLD, "CLD", "Death of a child"},
	{SIGPWR, "PWR", "Power-fail restart"},
#ifdef JOBS			/* todo: need to be more portable */
	{SIGTSTP, "TSTP", "Stop"},
	{SIGTTIN, "TTIN", "Stop (tty input)"},
#ifdef SIGPOLL
	{SIGPOLL, "POLL", "Pollable event occured"},
#endif
	{SIGSTOP, "STOP", "Stop (signal)"},
	{SIGTTOU, "TTOU", "Stop (tty output)"},
	{SIGCONT, "CONT", "Continue"},
#endif
#else
#ifdef JOBS			/* todo: need to be more portable */
	{SIGURG, "URG", "Urgent condition"}, /* BSDism */
	{SIGSTOP, "STOP", "Stop (signal)"},
	{SIGTSTP, "TSTP", "Stop"},
	{SIGCONT, "CONT", "Continue"},
	{SIGCHLD, "CHLD", "Waiting children"},
	{SIGTTIN, "TTIN", "Stop (tty input)"},
	{SIGTTOU, "TTOU", "Stop (tty output)"},
#endif
#endif
};

Trap *
gettrap(name)
	char *name;
{
	int i;
	register Trap *p;

	if (digit(*name)) {
		i = getn(name);
		return (0 <= i && i < SIGNALS) ? &sigtraps[getn(name)] : NULL;
	}
#if 0
	if (strcmp("ERR", name) == 0)
		return &e.loc->err;
	if (strcmp("EXIT", name) == 0)
		return &e.loc->exit;
#endif
	for (p = sigtraps, i = SIGNALS; --i >= 0; p++)
		if (p->name != NULL && strcmp(p->name, name) == 0)
			return p;
	return NULL;
}

/*
 * trap signal handler
 */
void
trapsig(i)
	int i;
{
	trap = sigtraps[i].set = 1;
	if (i == SIGINT && e.type == E_PARSE)
		/* dangerous but necessary to deal with BSD silly signals */
		longjmp(e.jbuf, 1);
#ifdef USE_SIGACT
	sigaction(i, &Sigact_trap, NULL);
#else
	(void) signal(i, trapsig);
#endif
}

/*
 * run any pending traps
 */
runtraps()
{
	int i;
	register Trap *p;

	for (p = sigtraps, i = SIGNALS; --i >= 0; p++)
		if (p->set)
			runtrap(p);
	trap = 0;
}

runtrap(p)
	Trap *p;
{
	char *trapstr;

	p->set = 0;
	if ((trapstr = p->trap) == NULL)
		if (p->signal == SIGINT)
			unwind();	/* return to shell() */
		else
			return;
	if (p->signal == 0)	/* ??? */
		p->trap = 0;
	command(trapstr);
}
 
/* restore signals for children */
cleartraps()
{
	int i;
	register Trap *p;

	if ((p = sigtraps)->trap != NULL) {	/* Maybe put in exchild() */
		afree((void *)p->trap,APERM);	/* Necessary? */
		p->trap = NULL;
	}
	for (i = SIGNALS, p = sigtraps; --i >= 0; p++) {
		p->set = 0;
#ifdef USE_SIGACT
		if (p->ourtrap)
		{
		  sigaction(p->signal, &Sigact_ign, &Sigact);
		  if (Sigact.sa_handler != SIG_IGN)
		    sigaction(p->signal, &Sigact_dfl, NULL);
		}
#else
		if (p->ourtrap && signal(p->signal, SIG_IGN) != SIG_IGN)
			(void) signal(p->signal, SIG_DFL);
#endif
	}
}

ignoresig(i)
	int i;
{
#ifdef USE_SIGACT
  sigaction(i, &Sigact_ign, &Sigact);
  sigemptyset(&Sigact.sa_mask);
  Sigact.sa_flags = 0;

  if (Sigact.sa_handler != SIG_IGN)
    sigtraps[i].sig_dfl = 1;
#else
  if (signal(i, SIG_IGN) != SIG_IGN)
    sigtraps[i].sig_dfl = 1;
#endif
}

restoresigs()
{
	int i;
	register Trap *p;

	for (p = sigtraps, i = SIGNALS; --i >= 0; p++)
		if (p->sig_dfl) {
			p->sig_dfl = 0;
#ifdef USE_SIGACT
			sigaction(p->signal, &Sigact_dfl, NULL);
#else
			(void) signal(p->signal, SIG_DFL);
#endif
		}
}

