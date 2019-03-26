/*
 * Process and job control
 */
#ifndef lint
static char *RCSid = "$Id: jobs.c,v 1.4 1992/04/27 07:14:26 sjg Exp $";
#endif

/*
 * based on version by Ron Natalie, BRL
 * modified by Simon J. Gerraty <sjg@melb.bull.oz.au>
 *
 * TODO:
 *	change Proc table to Job table, with array of pids.
 *	make %+ be jobs, %- be jobs->next.
 *	do not JFREE members of pipeline.
 *	consider procs[] related critical sections.
 *	signal(SIGCHLD, j_sigchld) should be
 *	sigaction(SIGCHLD, sigchld, NULL),
 *	with sigchld.sa_flags = SA_RESTART.
 *	There is a simple work-around if there is no SA_RESTART.
 */

#include "stdh.h"
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/times.h>
#include <sys/wait.h>
#include "sh.h"
#ifdef JOBS
#ifdef _BSD
#include <sys/ioctl.h>
#else
#include "termios.h"
#endif
#endif

#ifdef _BSD
/*
 * These macros are for the benefit of SunOS 4.0.2 and 4.0.3
 * SunOS 4.1.1 already defines most of them.
 * Clearly these are aimed at SunOS, they may work for other
 * BSD systems but I can't promise.
 */
# ifndef WIFSTOPPED
#   define WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
# endif
# ifndef WIFSIGNALED
#   define WIFSIGNALED(x) ((x).w_stopval != WSTOPPED && (x).w_termsig != 0)
# endif
# ifndef WIFEXITED
#   define WIFEXITED(x)	((x).w_stopval != WSTOPPED && (x).w_termsig == 0)
# endif
# ifndef WSTOPSIG
#   define WSTOPSIG(x)	((x).w_stopsig)
# endif
# ifndef WTERMSIG
#   define WTERMSIG(x)	((x).w_termsig)
# endif
# ifndef WIFCORED
#   define WIFCORED(x)	((x).w_coredump)
# endif
# ifndef WEXITSTATUS
#   define WEXITSTATUS(x)	((x).w_retcode)
# endif
# ifndef HAVE_WAITPID
#   define	waitpid(pid, sp, opts)	wait3(sp, opts, (void*)NULL)
# endif
#else					/* not _BSD */
# ifndef WIFCORED
#   define	WIFCORED(x)	(!!((x)&0x80)) /* non-standard */
# endif
#endif

/* as of P1003.1 Draft 12.3:
 *	pid_t getpgrp(void);		// Get process group id
 *	pid_t setsid(void);		// Create session and Set process group id
 *	int setpgid(pid_t pid, pid_t pgid); // Set process group id for job control
 */


#ifdef JOBS
#ifdef _BSD			/* _BSD 4.* */
#define	setpgid(p, pg)	setpgrp(p, pg)
#define	getpgid(p)	getpgrp(p)
#define	tcsetpgrp(fd,p)	ioctl(fd, TIOCSPGRP, &(p))
#else				/* POSIX-compatible */
#define	getpgid(p)	getpgrp() /* 1003.1 stupidity */
#define	killpg(p, s)	kill(-(p), s)
#endif
#endif


#ifndef	SIGCHLD
#define	SIGCHLD	SIGCLD
#endif
#ifndef WAIT_T
#ifdef _BSD
#define WAIT_T union wait
#else
#define WAIT_T int
#endif
#endif
#ifndef SA_RESTART
#define SA_RESTART	0
#endif

typedef struct Proc Proc;
struct Proc {
	Proc   *next;		/* `procs' list link */
	int	job;		/* job number: %n */
	short	volatile state;	/* proc state */
	short	volatile notify; /* proc state has changed */
	Proc   *prev;		/* prev member of pipeline */
	pid_t	proc;		/* process id */
	pid_t	pgrp;		/* process group if flag[FMONITOR] */
	short	flags;		/* execute flags */
	WAIT_T	status;		/* wait status */
	clock_t	utime, stime;	/* user/system time when JDONE */
	char	com [48];	/* command */
};

/* proc states */
#define	JFREE	0		/* unused proc */
#define	JRUN	1		/* foreground */
#define JEXIT	2		/* exit termination */
#define	JSIGNAL	3		/* signal termination */
#define	JSTOP	4		/* stopped */

static	Proc *procs = NULL;	/* job/process table */

clock_t	j_utime, j_stime;	/* user and system time for last job a-waited */
#ifdef JOBS
# ifdef USE_SIGACT
sigset_t sm_default, sm_sigchld;	/* signal masks */
# else
static	int	sm_default, sm_sigchld;	/* signal masks */
# endif
static	int	our_pgrp;		/* shell's pgrp */
#endif
static	Proc   *j_lastj;		/* last proc created by exchild */
static	int	j_lastjob = 0;		/* last created job */
static	int	j_current = 0;		/* current job */
static	int	j_previous = 0;		/* previous job */

static int      j_waitj     ARGS((Proc *aj, int intr));
static void     j_print     ARGS((Proc *j));
static int      j_newjob    ARGS((void));
static Proc *   j_search    ARGS((int job));
static void	j_sigchld   ARGS((int sig));
  
/* initialize job control */
void
j_init()
{
#ifdef JOBS
# if defined(NTTYDISC) && defined(TIOCSETD)
	int ldisc = NTTYDISC;	/* BSD brain damage */

	if (ttyfd >= 0)
		ioctl(ttyfd, TIOCSETD, &ldisc);
# endif
	our_pgrp = getpgid(0);
	sigchld_caught = 0;
# ifdef USE_SIGACT
	sigemptyset(&sm_default);
	sigemptyset(&sm_sigchld);
	sigaddset(&sm_sigchld, SIGCHLD);
# else
	sm_default = 0;
	sm_sigchld = sigmask(SIGCHLD);
	_TRACE(5, ("j_init: sm_sigchld == 0x%x", sm_sigchld));
# endif
#endif 
#ifndef JOBS
# ifdef USE_SIGACT
	sigaction(SIGCHLD, &Sigact_dfl, NULL);
# else
#   ifdef _SYSV
	signal(SIGCHLD, SIG_DFL);	/* necessary ??? */
#   endif
# endif
#endif
}

/* job cleanup before shell exit */
void
j_exit()
{
	register Proc *j;
	int killed = 0;

#ifdef JOBS
	/* kill stopped jobs */
	for (j = procs; j != NULL; j = j->next)
		if (j->state == JSTOP) {
			killed ++;
			killpg(j->pgrp, SIGHUP);
			killpg(j->pgrp, SIGCONT);
		}
	if (killed)
		sleep(1);
#endif
	j_notify();

#ifdef JOBS
	if (flag[FMONITOR]) {
		flag[FMONITOR] = 0;
		j_change();
	}
#endif
}

#ifdef JOBS
/* turn job control on or off according to flag[FMONITOR] */
void
j_change()
{
#ifdef USE_SIGACT
	static struct sigaction old_tstp, old_ttin, old_ttou;
#else
	static handler_t old_tstp, old_ttin, old_ttou;
#endif
	if (flag[FMONITOR]) {
		if (ttyfd < 0) {
			flag[FMONITOR] = 0;
			shellf("job control requires tty\n");
			return;
		}
#ifdef USE_SIGACT
		Sigact.sa_handler = j_sigchld;
		sigemptyset(&Sigact.sa_mask);
		Sigact.sa_flags = SA_RESTART;
		sigaction(SIGCHLD, &Sigact, NULL);
		Sigact.sa_flags = 0;
		sigtraps[SIGCHLD].sig_dfl = 1; /* restore on fork */
		sigaction(SIGTSTP, &Sigact_ign, &old_tstp);
		sigtraps[SIGTSTP].sig_dfl = 1;
		sigaction(SIGTTIN, &Sigact_ign, &old_ttin);
		sigtraps[SIGTTIN].sig_dfl = 1;
		sigaction(SIGTTOU, &Sigact_ign, &old_ttou);
		sigtraps[SIGTTOU].sig_dfl = 1;
#else
		(void) signal(SIGCHLD, j_sigchld);
		sigtraps[SIGCHLD].sig_dfl = 1; /* restore on fork */
		old_tstp = signal(SIGTSTP, SIG_IGN);
		sigtraps[SIGTSTP].sig_dfl = 1;
		old_ttin = signal(SIGTTIN, SIG_IGN);
		sigtraps[SIGTTIN].sig_dfl = 1;
		old_ttou = signal(SIGTTOU, SIG_IGN);
		sigtraps[SIGTTOU].sig_dfl = 1;
#endif
#ifdef USE_SIGACT
		sigprocmask(SIG_SETMASK, &sm_default, NULL);
#else
		sigsetmask(sm_default);
#endif
		tcsetpgrp(ttyfd, our_pgrp);
	} else {
#ifdef USE_SIGACT
		sigaction(SIGCHLD, &Sigact_dfl, NULL);
		sigaction(SIGTSTP, &old_tstp, NULL);
		sigtraps[SIGTSTP].sig_dfl = 0;
		sigaction(SIGTTIN, &old_ttin, NULL);
		sigtraps[SIGTTIN].sig_dfl = 0;
		sigaction(SIGTTOU, &old_ttou, NULL);
		sigtraps[SIGTTOU].sig_dfl = 0;
#else
		(void) signal(SIGCHLD, SIG_DFL);
		(void) signal(SIGTSTP, old_tstp);
		sigtraps[SIGTSTP].sig_dfl = 0;
		(void) signal(SIGTTIN, old_ttin);
		sigtraps[SIGTTIN].sig_dfl = 0;
		(void) signal(SIGTTOU, old_ttou);
		sigtraps[SIGTTOU].sig_dfl = 0;
#endif
	}
}
#endif

/* execute tree in child subprocess */
int
exchild(t, flags)
	struct op *t;
	int flags;
{
	register int i;
	register Proc *j;
	int rv = 0;
	int forksleep;

	flags &= ~XFORK;
	if ((flags&XEXEC))
		return execute(t, flags);

	/* get free Proc entry */
	for (j = procs; j != NULL; j = j->next)
		if (j->state == JFREE)
			goto Found;
	j = (Proc*) alloc(sizeof(Proc), APERM);
	j->next = procs;
	j->state = JFREE;
	procs = j;
  Found:

	j->prev = ((flags&XPIPEI)) ? j_lastj : NULL;
	j->proc = j->pgrp = 0;
	j->flags = flags;
	j->job = (flags&XPIPEI) ? j_lastjob : j_newjob();
	snptreef(j->com, sizeof(j->com), "%T", t); /* save proc's command */
	j->com[sizeof(j->com)-1] = '\0';
	j->state = JRUN;

	/* stdio buffer must be flushed and invalidated */
	for (i = 0; i < NUFILE; i++)
		flushshf(i);

	/* create child process */
	forksleep = 0;
#ifdef JOBS
	/* don't allow SIGCHLD until we are ready */

#ifdef USE_SIGACT
	sigprocmask(SIG_SETMASK, &sm_sigchld, NULL);
# else
	sigsetmask(sm_sigchld);
# endif
#endif
	while ((i = fork()) < 0 && errno == EAGAIN && forksleep < 32) {
		if (forksleep) {
			sleep(forksleep);
			forksleep <<= 1;
		} else
			forksleep = 1;
	}
	if (i < 0) {
		j->state = JFREE;
		errorf("cannot fork - try again\n");
	}
	j->proc = (i != 0) ? i : getpid();

#ifdef JOBS
	/* job control set up */
	if (flag[FMONITOR] && !(flags&XXCOM))
	{
	  j->pgrp = !(flags&XPIPEI) ? j->proc : j_lastj->pgrp;
	  /* do in both parent and child to avoid fork race condition */
	  if (!(flags&XBGND))
	    tcsetpgrp(ttyfd, j->pgrp); /* could be trouble */
	  setpgid(j->proc, j->pgrp);
	}
#endif
	j_lastj = j;

	if (i == 0) {		/* child */
		e.oenv = NULL;
		if (flag[FTALKING])
			restoresigs();
		if ((flags&XBGND) && !flag[FMONITOR])
		{
#ifdef USE_SIGACT
		  sigaction(SIGINT, &Sigact_dfl, NULL);
		  sigaction(SIGQUIT, &Sigact_dfl, NULL);
		  if (flag[FTALKING])
		    sigaction(SIGTERM, &Sigact_dfl, NULL);
#else
		  signal(SIGINT, SIG_IGN);
		  signal(SIGQUIT, SIG_IGN);
		  if (flag[FTALKING])
		    signal(SIGTERM, SIG_DFL);
#endif
			if (!(flags&XPIPEI)) {
				i = open("/dev/null", 0);
				(void) dup2(i, 0);
				close(i);
			}
		}
		for (j = procs; j != NULL; j = j->next)
			j->state = JFREE;
		ttyfd = -1;
#ifdef JOBS
		/* is this needed in the child? */
# ifdef USE_SIGACT
		sigprocmask(SIG_SETMASK, &sm_default, NULL);
# else
		sigsetmask(sm_default);
# endif
#endif
		flag[FMONITOR] = flag[FTALKING] = 0;
		cleartraps();
		execute(t, flags|XEXEC); /* no return */
		/* NOTREACHED */
	}

	/* shell (parent) stuff */
	if ((flags&XBGND)) { /* async statement */
		async = j->proc;
		j_previous = j_current;
		j_current = j->job;
		if (flag[FTALKING])
			j_print(j);
	}
#ifdef JOBS
# ifdef USE_SIGACT
	sigprocmask(SIG_SETMASK, &sm_default, NULL);
# else
	sigsetmask(sm_default);
# endif
#endif
	if (!(flags&XBGND))
	{ 		/* sync statement */
		if (!(flags&XPIPE))
			rv = j_waitj(j, 0);
	}

	return rv;
}

/* wait for last job: pipeline or $() sub-process */
int
waitlast()
{
	return j_waitj(j_lastj, 0);
}

/* wait for job to complete or change state */
static int
j_waitj(aj, intr)
	Proc *aj;
	int intr;		/* interruptable */
{
	register Proc *j;
	int rv = 1;
	int ttysig = 0;

#ifdef JOBS
	if (flag[FMONITOR])
	{
# ifdef USE_SIGACT
	  sigprocmask(SIG_SETMASK, &sm_sigchld, NULL);
# else
	  _TRACE(5, ("j_waitj: sigsetmask(sm_sigchld==0x%x)", sm_sigchld));
	  sigsetmask(sm_sigchld);
# endif
	}
#endif
	/* wait for all members of pipeline */
	for (j = aj; j != NULL; j = j->prev) {
		/* wait for job to finish, stop, or ^C of built-in wait */
		while (j->state == JRUN) {
#ifdef JOBS
			if (flag[FMONITOR])
			{
			  /*
			   * 91-07-07 <sjg@sun0>
			   * we don't want to wait for a signal
			   * that has already arrived :-)
			   */
			  if (!sigchld_caught)
			  {
# ifdef USE_SIGACT
			    sigsuspend(&sm_default);
# else
			    _TRACE(4, ("j_waitj: sigpause(%d), sigchld_caught==%d", sm_default, sigchld_caught));
			    sigpause(sm_default);
			    _TRACE(4, ("j_waitj: sigpause() returned %d, sigchld_caught==%d", errno, sigchld_caught));
# endif /* USE_SIGACT */
			  }
			}
			else
#endif /* JOBS */
				j_sigchld(0);
			/*
			 * Children to reap
			 */
			if (sigchld_caught)
			  j_reapchld();
			_TRACE(4, ("j_waitj: j->proc==%d, j->com=='%s', j->state==0x%hx, j->status==0x%x, j->notify==%hd", j->proc, j->com, j->state, j->status, j->notify));
			
			if (sigtraps[SIGINT].set && intr)
				goto Break;
		}
		if (j->state == JEXIT) { /* exit termination */
			if (!(j->flags&XPIPEO))
				rv = WEXITSTATUS(j->status);
			j->notify = 0;
		} else
		if (j->state == JSIGNAL) { /* signalled to death */
			if (!(j->flags&XPIPEO))
				rv = 0x80 + WTERMSIG(j->status);
			if (WTERMSIG(j->status) == SIGINT ||
			    (WTERMSIG(j->status) == SIGPIPE &&
			     (j->flags&XPIPEO)))
				j->notify = 0;
			if (WTERMSIG(j->status) == SIGINT ||
			    WTERMSIG(j->status) == SIGQUIT)
				ttysig = 1;
		} else
#ifdef JOBS
		if (j->state == JSTOP)
			if (WSTOPSIG(j->status) == SIGTSTP)
				ttysig = 1;
#else
		;
#endif
	}

	/* compute total child time for time statement */
	for (j = aj; j != NULL; j = j->prev)
		j_utime += j->utime, j_stime += j->stime;

	/* find new current job */
#ifdef JOBS
	if (aj->state == JSTOP) {
		j_previous = j_current;
		j_current = aj->job;
	} else {
#else
	if (1) {
#endif
		int hijob = 0;

		/* todo: this needs to be done in j_notify */
		/* todo: figure out what to do with j_previous */
		j_current = 0;
		for (j = procs; j != NULL; j = j->next)
			if ((j->state == JRUN || j->state == JSTOP)
			    && j->job > hijob) {
				hijob = j->job;
				j_current = j->job;
			}
	}

  Break:
#ifdef JOBS
	if (flag[FMONITOR])
	{
	  /* reset shell job control state */
# ifdef USE_SIGACT
	  sigprocmask(SIG_SETMASK, &sm_default, NULL);
# else
	  sigsetmask(sm_default);
# endif
	  tcsetpgrp(ttyfd, our_pgrp);
	}
#endif
	if (ttysig)
		fputc('\n', shlout);
	j_notify();

	return rv;
}

/* SIGCHLD handler to reap children */
/*
 * 91-07-07 <sjg@sun0>
 * On the Sun SS2 this appears to get called
 * too quickly!
 * So just record the event and process later.
 */
static void
j_sigchld(sig)
	int sig;
{
	sigchld_caught++;	/* acknowledge it */
}

/*
 * 91-07-07 <sjg@sun0>
 * This now gets called when j_sigchld()
 * has recorded some signals...
 */
j_reapchld()
{
	struct tms t0, t1;
#if defined(JOBS)
# ifdef USE_SIGACT
	sigset_t	sm_old;

	sigprocmask(SIG_SETMASK, NULL, &sm_old);
# else
	int sm_old;

	sm_old = sigblock(0);	/* just get current mask */
	_TRACE(5, ("j_reapchld: sm_old==0x%x, sigchld_caught==%d", sm_old, sigchld_caught));
# endif
#endif
	(void) times(&t0);

	do {
		register Proc *j;
		int pid;
		WAIT_T status;
#ifdef JOBS
		if (flag[FMONITOR])
			pid = waitpid(-1, &status, (WNOHANG|WUNTRACED));
		else
#endif
			pid = wait(&status);
		if (pid < 0 && errno == ECHILD)
		{
		  /* no children - what are we doing here? */
		  _TRACE(5, ("j_reapchld: no children"));
		  sigchld_caught = 0;
		  break;
		}
		if (pid <= 0)	/* return if would block (0) ... */
			break;	/* ... or no children or interrupted (-1) */
		(void) times(&t1);

		_TRACE(5, ("j_reapchld: looking for pid==%d", pid));

		for (j = procs; j != NULL; j = j->next)
		{
		  _TRACE(6, ("j_reapchld: j->proc==%d, j->com=='%s', j->state==0x%hx, j->status==0x%x, j->notify==%hd", j->proc, j->com, j->state, j->status, j->notify));
		  if (j->state != JFREE && j->proc == pid)
		    goto Found;
		}
		_TRACE(5, ("j_reapchld: did not find pid==%d", pid));
		continue;
	  Found:
		_TRACE(5, ("j_reapchld: found pid==%d", pid));
		j->notify = 1;
		j->status = status;
#ifdef JOBS
		if (WIFSTOPPED(status))
			j->state = JSTOP;
		else
#endif
		if (WIFEXITED(status))
			j->state = JEXIT;
		else
		if (WIFSIGNALED(status))
			j->state = JSIGNAL;

		/* compute child's time */
		/* todo: what does a stopped job do? */
		j->utime = t1.tms_cutime - t0.tms_cutime;
		j->stime = t1.tms_cstime - t0.tms_cstime;
		t0 = t1;
#ifdef JOBS
# ifdef USE_SIGACT
		sigprocmask(SIG_BLOCK, &sm_sigchld, NULL);
# else
		sigblock(sm_sigchld);
# endif
#endif
		if (--sigchld_caught < 0) /* reduce the count */
		  sigchld_caught = 0;
#ifdef JOBS
# ifdef USE_SIGACT
		sigprocmask(SIG_SETMASK, &sm_old, NULL);
# else
		_TRACE(5, ("j_reapchld: j->proc==%d, j->com=='%s', j->state==0x%hx, j->status==0x%x, j->notify==%hd", j->proc, j->com, j->state, j->status, j->notify));
		sigsetmask(sm_old); /* restore old mask */
# endif
#endif
		
#ifdef JOBS
	} while (flag[FMONITOR]);
#else
	} while (0);		/* only once if wait()ing */
#endif
/*
 * this should be safe
 */
#if defined(_SYSV) && !defined(JOBS) && !defined(USE_SIGACT)
	signal(SIGCHLD, j_sigchld);
#if 0
	/* why was this here??? */
	signal(SIGCLD, SIG_DFL);
#endif
#endif
}

j_reap()
{
  if (sigchld_caught)
    j_reapchld();
/*
 * now done in j_reapchld()
 */
#if 0 && defined(_SYSV) && !defined(JOBS) && !defined(USE_SIGACT)
	signal(SIGCHLD, j_sigchld);
	signal(SIGCLD, SIG_DFL);
#endif
	return(0);
}

/* wait for child, interruptable */
int
waitfor(job)
	int job;
{
	register Proc *j;

	if (job == 0 && j_current == 0)
		errorf("no current job\n");
	j = j_search((job == 0) ? j_current : job);
	if (j == NULL)
		errorf("no such job: %d\n", job);
	if (flag[FTALKING])
		j_print(j);
	if (e.interactive) {	/* flush stdout, shlout */
		fflush(shf[1]);
		fflush(shf[2]);
	}
	return j_waitj(j, 1);
}

/* kill (built-in) a job */
void
j_kill(job, sig)
	int job;
	int sig;
{
	register Proc *j;

	j = j_search(job);
	if (j == NULL)
		errorf("cannot find job\n");
	if (j->pgrp == 0) {	/* !flag[FMONITOR] */
		if (kill(j->proc, sig) < 0) /* todo: all member of pipeline */
			errorf("kill: %s\n", strerror(errno));
#ifdef JOBS
	} else {
		if (sig == SIGTERM || sig == SIGHUP)
			(void) killpg(j->pgrp, SIGCONT);
		if (killpg(j->pgrp, sig) < 0)
			errorf("killpg: %s\n", strerror(errno));
#endif
	}
}

#ifdef JOBS

/* fg and bg built-ins */
int
j_resume(job, bg)
	int job;
	int bg;
{
	register Proc *j; 
	
	j = j_search((job == 0) ? j_current : job);
	if (j == NULL)
		errorf("cannot find job\n", job);
	if (j->pgrp == 0)
		errorf("job not job-controlled\n");

	j->state = JRUN;
	j_print(j);
	flushshf(2);

	if (!bg)
  		tcsetpgrp(ttyfd, j->pgrp); /* attach shell to job */
	if (killpg(j->pgrp, SIGCONT) < 0)
		errorf("cannot continue job %%%d\n", job);
	if (!bg)
		return j_waitj(j, 0);
	return 0;
}

#endif

/* list jobs for jobs built-in */
void
j_jobs()
{
	register Proc *j; 

	for (j = procs; j != NULL; j = j->next)
		if (j->state != JFREE)
			j_print(j);
}

/* list jobs for top-level notification */
void
j_notify()
{
	register Proc *j; 

	/*
	 * since reaping is no longer done in the signal handler
	 * we had better try here...
	 */
	if (sigchld_caught)
	  j_reapchld();
	
	for (j = procs; j != NULL; j = j->next) {
		if (j->state == JEXIT && !flag[FTALKING])
			j->notify = 0;
		if (j->state != JFREE && j->notify)
			j_print(j);
		if (j->state == JEXIT || j->state == JSIGNAL)
			j->state = JFREE;
		j->notify = 0;
	}
}

static void
j_print(j)
	register Proc *j;
{
	char buf [64], *s = buf;

	switch (j->state) {
	  case JRUN:
		s = "Running";
		break;

#ifdef JOBS
	  case JSTOP:
		strcpy(buf, "Stopped ");
		s = strchr(sigtraps[WSTOPSIG(j->status)].mess, '(');
		if (s != NULL)
			strcat(buf, s);
		s = buf;
		break;
#endif

	  case JEXIT: {
		int rv;
		rv = WEXITSTATUS(j->status);
		sprintf(buf, "Done (%d)", rv);
		if (rv == 0)
			*strchr(buf, '(') = 0;
		j->state = JFREE;
		} break;

	  case JSIGNAL: {
		int sig = WTERMSIG(j->status);
		char *n = sigtraps[sig].mess;
		if (n != NULL)
			sprintf(buf, "%s", n);
		else
			sprintf(buf, "Signal %d", sig);
		if (WIFCORED(j->status))
			strcat(buf, " - core dumped");
		j->state = JFREE;
		} break;

	  default:
		s = "Hideous job state";
		j->state = JFREE;
		break;
	}
	shellf("%%%-2d%c %5d %-20s %s%s\n", j->job,
	       (j_current==j->job) ? '+' : (j_previous==j->job) ? '-' : ' ',
	       j->proc, s, j->com, (j->flags&XPIPEO) ? "|" : "");
}

/* convert % sequence to job number */
int
j_lookup(cp)
	char *cp;
{
	register Proc *j;
	int len, job = 0;

	if (*cp == '%')		/* leading % is optional */
		cp++;
	switch (*cp) {
	  case '\0':
	  case '+':
		job = j_current;
		break;

	  case '-':
		job = j_previous;
		break;

	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9': 
		job = atoi(cp);
		break;

	  case '?':		/* %?string */
		for (j = procs; j != NULL; j = j->next)
			if (j->state != JFREE && strstr(j->com, cp+1) != NULL)
				job = j->job;
		break;

	  default:		/* %string */
		len = strlen(cp);
		for (j = procs; j != NULL; j = j->next)
			if (j->state != JFREE && strncmp(cp, j->com, len) == 0)
				job = j->job;
		break;
	}
	if (job == 0)
		errorf("%s: no such job\n", cp);
	return job;
}

/* are any stopped jobs ? */
#ifdef JOBS
int
j_stopped()
{
	register Proc *j; 

	for (j = procs; j != NULL; j = j->next)
		if (j->state == JSTOP)
			return 1;
	return 0;
}
#endif

/* create new job number */
static int
j_newjob()
{
	register Proc *j; 
	register int max = 0;
	
	j_lastjob ++;
	for (j = procs; j != NULL; j = j->next)
		if (j->state != JFREE && j->job)
			if (j->job > max)
				max = j->job;
	if (j_lastjob > max)
		j_lastjob = max + 1;
	return j_lastjob;
}

/* search for job by job number */
static Proc *
j_search(job)
	int job;
{
	register Proc *j;

	for (j = procs; j != NULL; j = j->next)
		if (j->state != JFREE && job == j->job && !(j->flags&XPIPEO))
			return j;
	return NULL;
}

