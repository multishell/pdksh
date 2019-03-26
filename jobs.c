/*
 * Process and job control
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: jobs.c,v 1.2 1994/05/19 18:32:40 michael Exp michael $";
#endif

/*
 * Reworked/Rewritten version of Eric Gisin's/Ron Natalie's code by
 * Larry Bouzane (larry@cs.mun.ca) and hacked again by
 * Michael Rendell (michael@cs.mun.ca)
 *
 * The interface to the rest of the shell should probably be changed
 * to allow use of vfork() when available but that would be way too much
 * work :)
 */

#include "sh.h"
#include "ksh_wait.h"
#include "ksh_times.h"
#include "tty.h"

/* Start of system configuration stuff */

/* We keep CHILD_MAX zombie processes around (exact value isn't critical) */
#ifndef CHILD_MAX
# if defined(HAVE_SYSCONF) && defined(_SC_CHILD_MAX)
#  define CHILD_MAX sysconf(_SC_CHILD_MAX)
# else /* _SC_CHILD_MAX */
#  ifdef _POSIX_CHILD_MAX
#   define CHILD_MAX	((_POSIX_CHILD_MAX) * 2)
#  else /* _POSIX_CHILD_MAX */
#   define CHILD_MAX	20
#  endif /* _POSIX_CHILD_MAX */
# endif /* _SC_CHILD_MAX */
#endif /* !CHILD_MAX */

#ifdef	JOBS
# if !defined(HAVE_WAITPID) && defined(HAVE_WAIT3)
#  define waitpid(p, s, o)	wait3((s), (o), (struct rusage *) 0)
# endif

# if defined(HAVE_TCSETPGRP) || defined(TIOCSPGRP)
#  define TTY_PGRP
# endif
# ifdef BSD_PGRP
#  define setpgid	setpgrp
#  define getpgID()	getpgrp(0)
# else
#  define getpgID()	getpgrp()
# endif
# if !defined(HAVE_TCSETPGRP) && defined(TIOCSPGRP)
int tcsetpgrp ARGS((int fd, pid_t grp));
int tcgetpgrp ARGS((int fd));

int
tcsetpgrp(fd, grp)
	int fd;
	pid_t grp;
{
	return ioctl(fd, TIOCSPGRP, &grp);
}

int
tcgetpgrp(fd)
	int	fd;
{
	int r, grp;

	if ((r = ioctl(fd, TIOCGPGRP, &grp)) < 0)
		return r;
	return grp;
}
# endif	/* !HAVE_TCSETPGRP && TIOCSPGRP */
#endif /* JOBS */

/* End of system configuration stuff */


/* Order important! */
#define	PRUNNING	0
#define PEXITED		1
#define PSIGNALLED	2
#define PSTOPPED	3

typedef struct proc	Proc;
struct proc {
	Proc	*next;		/* next process in pipeline (if any) */
	int	state;
	WAIT_T	status;		/* wait status */
	pid_t	pid;		/* process id */
	char	command[48];	/* process command string */
};

/* Notify/print flag - j_print() argument */
#define JP_NONE		0	/* don't print anything */
#define JP_SHORT	1	/* print signals processes were killed by */
#define JP_MEDIUM	2	/* print [job-num] -/+ command */
#define JP_LONG		3	/* print [job-num] -/+ pid command */
#define JP_PGRP		4	/* print pgrp */

/* put_job() flags */
#define PJ_ON_FRONT	0	/* at very front */
#define PJ_PAST_STOPPED	1	/* just past any stopped jobs */

/* Job.flags values */
#define JF_STARTED	0x001	/* set when all processes in job are started */
#define JF_WAITING	0x002	/* set if j_waitj() is waiting on job */
#define JF_W_ASYNCNOTIFY 0x004	/* set if waiting and async notification ok */
#define JF_XXCOM	0x008	/* set for `command` jobs */
#define JF_FG		0x010	/* running in forground (also has tty pgrp) */
#define JF_SAVEDTTY	0x020	/* j->ttystate is valid */
#define JF_CHANGED	0x040	/* process has changed state */
#define JF_KNOWN	0x080	/* $! referenced */
#define JF_ZOMBIE	0x100	/* known, unwaited process */
#define JF_REMOVE	0x200	/* flaged for removal (j_jobs()/j_noityf()) */

typedef struct job Job;
struct job {
	Job	*next;		/* next job in list */
	int	job;		/* job number: %n */
	int	flags;		/* see JF_* */
	int	state;		/* job state */
	int	status;		/* exit status of last process */
	pid_t	pgrp;		/* process group of job */
	pid_t	ppid;		/* pid of process that forked job */
	INT32	age;		/* number of jobs started */
	clock_t	stime;		/* system time used by job */
	clock_t	utime;		/* user time used by job */
	Proc	*proc_list;	/* process list */
	Proc	*last_proc;	/* last process in list */
#if defined(JOBS) && defined(TTY_PGRP)
	TTY_state ttystate;	/* saved tty state for stopped jobs */
#endif	/* JOBS && TTY_PGRP */
};

/* Flags for j_waitj() */
#define JW_NONE		0x00
#define JW_INTERRUPT	0x01	/* ^C will stop the wait */
#define JW_ASYNCNOTIFY	0x02	/* asynchronous notification during wait ok */
#define JW_STOPPEDWAIT	0x04	/* wait even if job stopped */

/* Error codes for j_lookup() */
#define JL_OK		0
#define JL_NOSUCH	1	/* no such job */
#define JL_AMBIG	2	/* %foo or %?foo is ambiguous */
#define JL_INVALID	3	/* non-pid, non-% job id */

static char	*lookup_msgs[] = {
				"",
				"no such job",
				"ambiguous",
				"argument must be %job or process id",
				(char *) 0
			    };
clock_t	j_stime, j_utime;	/* user and system time of last j_waitjed job */

static Job		*job_list = (Job *) 0;	/* job list */
static Job		*last_job = (Job *) 0;
static Job		*async_job = (Job *) 0;
static pid_t		async_pid;

static int		nzombie;	/* # of zombies owned by this process */
static INT32		njobs;		/* # of jobs started */
static int		child_max;	/* CHILD_MAX */


#ifdef	JOBS
static sigset_t		sm_default, sm_sigchld;
static struct shf	*shl_j;
static int		held_sigchld;	/* set when sigchld occurs before job started */
# ifdef TTY_PGRP
static int		ttypgrp_ok;	/* set if can use tty pgrps */
static pid_t		restore_ttypgrp = -1;
static pid_t		our_pgrp;
static int		tt_sigs[] = { SIGTSTP, SIGTTIN, SIGTTOU };
# endif /* TTY_PGRP */
# ifdef NEED_PGRP_SYNC
/* Some systems, the kernel doesn't count zombie processes when checking
 * if a process group is valid, which can cause problems in creating the
 * pipeline "cmd1 | cmd2": if cmd1 can die (and go into the zombie state)
 * before cmd2 is started, the kernel doesn't allow the setpgrp() for cmd2
 * to succeed.  Solution is to create a pipe between the parent and the first
 * process; the first process doesn't do anything until the pipe is closed
 * and the parent doesn't close the pipe until all the processes are started.
 */
static int		j_sync_pipe[2];
static int		j_sync_open;
# endif /* NEED_PGRP_SYNC */
#endif	/* JOBS */

static void		j_set_async ARGS((Job *j));
static void		j_startjob ARGS((Job *j));
static int		j_waitj ARGS((Job *j, int flags, char *where));
static RETSIGTYPE	j_sigchld ARGS((int sig));
static void		j_print ARGS((Job *j, int how, struct shf *shf));
static Job		*j_lookup ARGS((char *cp, int *ecodep));
static Job		*new_job ARGS((void));
static Proc		*new_proc ARGS((void));
static void		check_job ARGS((Job *j));
static void		put_job ARGS((Job *j, int where));
static void		remove_job ARGS((Job *j, char *where));
static void		kill_job ARGS((Job *j));
static void	 	fill_command ARGS((char *c, int len, struct op *t));

/* initialize job control */
void
j_init(mflagset)
	int mflagset;
{
	child_max = CHILD_MAX; /* so syscon() isn't always being called */

#ifdef	JOBS
	if (!mflagset && Flag(FTALKING))
		Flag(FMONITOR) = 1;

	/* shl_j is used to do asynchronous notification (used in
	 * an interrupt handler, so need a distinct shf)
	 */
	shl_j = shf_fdopen(2, SHF_WR, (struct shf *) 0);

	sigemptyset(&sm_default);
	sigprocmask(SIG_SETMASK, &sm_default, (sigset_t *) 0);

	sigemptyset(&sm_sigchld);
	sigaddset(&sm_sigchld, SIGCHLD);

	setsig(&sigtraps[SIGCHLD], j_sigchld, SS_RESTORE_ORIG|SS_FORCE);

# ifdef TTY_PGRP
	if (Flag(FMONITOR) || Flag(FTALKING)) {
		int i;

		/* j_change() sets these to SS_RESTORE_DFL if FMONITOR */
		for (i = 3; --i >= 0; ) {
			sigtraps[tt_sigs[i]].flags |= TF_SHELL_USES;
			setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
		}
	}
# endif /* TTY_PGRP */

	/* j_change() calls tty_init() */
	if (Flag(FMONITOR))
		j_change();
	else if (Flag(FTALKING))
		tty_init(TRUE);
#else	/* JOBS */
	if (Flag(FTALKING))
		tty_init(TRUE);
	/* Make sure SIGCHLD isn't ignored - can do odd things under SYSV */
	setsig(&sigtraps[SIGCHLD], SIG_DFL, SS_RESTORE_ORIG|SS_FORCE);
#endif	/* JOBS */
}

/* job cleanup before shell exit */
void
j_exit()
{
#ifdef	JOBS
	/* kill stopped jobs */
	Job	*j;
	int	killed = 0;

	for (j = job_list; j != (Job *) 0; j = j->next)
		if (j->ppid == procpid && j->state == PSTOPPED) {
			killed ++;
			killpg(j->pgrp, SIGCONT);
			killpg(j->pgrp, SIGHUP);
		}
	if (killed)
		sleep(1);
#endif	/* JOBS */
	j_notify();

#ifdef 	JOBS
# ifdef TTY_PGRP
	if (kshpid == procpid && restore_ttypgrp >= 0)
		tcsetpgrp(tty_fd, restore_ttypgrp);
# endif /* TTY_PGRP */
	if (Flag(FMONITOR)) {
		Flag(FMONITOR) = 0;
		j_change();
	}
#endif 	/* JOBS */
}

#ifdef	JOBS
/* turn job control on or off according to Flag(FMONITOR) */
void
j_change()
{
	int i;

	if (Flag(FMONITOR)) {
		/* Don't call get_tty() 'til we own the tty process group */
		tty_init(FALSE);

# ifdef TTY_PGRP
		/* no controlling tty, no SIGT* */
		ttypgrp_ok = tty_fd >= 0 && tty_devtty;

		if (ttypgrp_ok && (our_pgrp = getpgID()) < 0) {
			shellf("j_init: getpgrp() failed: %s\n",
				strerror(errno));
			ttypgrp_ok = 0;
		}
		if (ttypgrp_ok) {
			setsig(&sigtraps[SIGTTIN], SIG_DFL,
				SS_RESTORE_ORIG|SS_FORCE);
			/* wait to be given tty (POSIX.1, B.2, job control) */
			while (1) {
				pid_t ttypgrp;

				if ((ttypgrp = tcgetpgrp(tty_fd)) < 0) {
					shellf(
					    "j_init: tcgetpgrp() failed: %s\n",
						strerror(errno));
					ttypgrp_ok = 0;
					break;
				}
				if (ttypgrp == our_pgrp)
					break;
				kill(0, SIGTTIN);
			}
		}
		for (i = 3; --i >= 0; )
			setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
				SS_RESTORE_DFL|SS_FORCE);
		if (ttypgrp_ok && our_pgrp != kshpid) {
			if (setpgid(0, kshpid) < 0) {
				shellf("j_init: setpgid() failed: %s\n",
					strerror(errno));
				ttypgrp_ok = 0;
			} else {
				if (tcsetpgrp(tty_fd, kshpid) < 0) {
					shellf(
					    "j_init: tcsetpgrp() failed: %s\n",
						strerror(errno));
					ttypgrp_ok = 0;
				} else
					restore_ttypgrp = our_pgrp;
				our_pgrp = kshpid;
			}
		}
#  if defined(NTTYDISC) && defined(TIOCSETD) && !defined(HAVE_TERMIOS_H) && !defined(HAVE_TERMIO_H)
		if (ttypgrp_ok) {
			int ldisc = NTTYDISC;

			if (ioctl(tty_fd, TIOCSETD, &ldisc) < 0)
				shellf(
				  "j_init: can't set new line discipline: %s\n",
					strerror(errno));
		}
#  endif /* NTTYDISC && TIOCSETD */
		if (!ttypgrp_ok)
			shellf("warning: won't have full job control\n");
		shf_flush(shl_out);
# endif /* TTY_PGRP */
		if (tty_fd >= 0)
			get_tty(tty_fd, &tty_state);
	} else {
# ifdef TTY_PGRP
		ttypgrp_ok = 0;
		/* the TF_SHELL_USES test is a kludge that lets us know if
		 * if the signals have been changed by the shell.
		 */
		if (Flag(FTALKING))
			for (i = 3; --i >= 0; )
				setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
					SS_RESTORE_IGN|SS_FORCE);
		else
			for (i = 3; --i >= 0; ) {
				if (sigtraps[tt_sigs[i]].flags & (TF_ORIG_IGN
							          |TF_ORIG_DFL))
					setsig(&sigtraps[tt_sigs[i]],
						(sigtraps[tt_sigs[i]].flags & TF_ORIG_IGN) ? SIG_IGN : SIG_DFL,
						SS_RESTORE_CURR|SS_FORCE);
			}
# endif /* TTY_PGRP */
		if (!Flag(FTALKING))
			tty_close();
	}
}
#endif	/* JOBS */

/* execute tree in child subprocess */
int
exchild(t, flags, close_fd)
	struct op	*t;
	int		flags;
	int		close_fd;	/* used if XPCLOSE or XCCLOSE */
{
	static Proc	*last_proc;	/* for pipelines */

	int		i;
#ifdef	JOBS
	sigset_t	omask;
#endif	/* JOBS */
	Proc		*p;
	Job		*j;
	int		rv = 0;
	int		forksleep;
	int		orig_flags = flags;
	int		ischild;

	flags &= ~(XFORK|XPCLOSE|XCCLOSE);
	if ((flags&XEXEC))
		return execute(t, flags);

#ifdef	JOBS
	/* no SIGCHLD's while messing with job and process lists */
	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	p = new_proc();
	p->next = (Proc *) 0;
	p->state = PRUNNING;
	WSTATUS(p->status) = 0;
	p->pid = 0;

	/* link process into jobs list */
	if (flags&XPIPEI) {	/* continuing with a pipe */
		j = last_job;
		last_proc->next = p;
		last_proc = p;
	} else {
#if defined(JOBS) && defined(NEED_PGRP_SYNC)
		if (j_sync_open) {
			closepipe(j_sync_pipe);
			j_sync_open = 0;
		}
		/* don't do the sync pipe business if there is no pipeline */
		if (flags & XPIPEO) {
			openpipe(j_sync_pipe);
			j_sync_open = 1;
		}
#endif /* JOBS && NEED_PGRP_SYNC */
		j = new_job(); /* fills in j->job */
		/* we don't consider XXCOM's foreground since they don't get
		 * tty process group and we don't save or restore tty modes.
		 */
		j->flags = (flags & XXCOM) ? JF_XXCOM
			: ((flags & XBGND) ? 0 : JF_FG);
		j->utime = j->stime = 0;
		j->state = PRUNNING;
		j->pgrp = 0;
		j->ppid = procpid;
		j->age = ++njobs;
		j->proc_list = p;
		last_job = j;
		last_proc = p;
		if (flags & XXCOM)
			j->flags |= JF_XXCOM;
		else if (!(flags & XBGND))
			j->flags |= JF_FG;
		put_job(j, PJ_PAST_STOPPED);
	}

	fill_command(p->command, sizeof(p->command), t);

	/* create child process */
	forksleep = 1;
	while ((i = fork()) < 0 && errno == EAGAIN && forksleep < 32) {
		sleep(forksleep);
		forksleep <<= 1;
	}
	if (i < 0) {
		kill_job(j);
		remove_job(j, "fork failed");
#ifdef	JOBS
# ifdef NEED_PGRP_SYNC
		if (j_sync_open) {
			closepipe(j_sync_pipe);
			j_sync_open = 0;
		}
# endif /* NEED_PGRP_SYNC */
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
		errorf("cannot fork - try again\n");
	}
	ischild = i == 0;
	if (ischild)
		p->pid = procpid = getpid();
	else
		p->pid = i;

#ifdef	JOBS
	/* job control set up */
	if (Flag(FMONITOR) && !(flags&XXCOM)) {
		int	dotty = 0;
# ifdef NEED_PGRP_SYNC
		int	dosync = 0;
# endif /* NEED_PGRP_SYNC */

		if (j->pgrp == 0) {	/* First process */
			j->pgrp = p->pid;
			dotty = 1;
# ifdef NEED_PGRP_SYNC
			if (j_sync_open) {
				close(j_sync_pipe[ischild ? 1 : 0]);
				j_sync_pipe[ischild ? 1 : 0] = -1;
				dosync = ischild;
			}
# endif /* NEED_PGRP_SYNC */
		}

		/* set pgrp in both parent and child to deal with race
		 * condition
		 */
		setpgid(p->pid, j->pgrp);
# ifdef TTY_PGRP
		/* YYY: should this be
		   if (ttypgrp_ok && ischild && !(flags&XBGND))
			tcsetpgrp(tty_fd, j->pgrp);
		   instead? (see also YYY below)
		 */
		if (ttypgrp_ok && dotty && !(flags & XBGND))
			tcsetpgrp(tty_fd, j->pgrp);
# endif /* TTY_PGRP */
# ifdef NEED_PGRP_SYNC
		if (ischild && j_sync_open) {
			if (dosync) {
				char c;
				while (read(j_sync_pipe[0], &c, 1) == -1
				       && errno == EINTR)
					;
			}
			close(j_sync_pipe[0]);
			j_sync_open = 0;
		}
# endif /* NEED_PGRP_SYNC */
	}
#endif	/* JOBS */

	/* used to close pipe input fd */
	if (close_fd >= 0 && (((orig_flags & XPCLOSE) && i != 0)
			      || ((orig_flags & XCCLOSE) && i == 0)))
		close(close_fd);
	if (i == 0) {		/* child */
#ifdef	JOBS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
		e->oenv = NULL;
#if defined(JOBS) && defined(TTY_PGRP)
		/* If FMONITOR or FTALKING is set, these signals are ignored,
		 * if neither FMONITOR nor FTALKING are set, the signals have
		 * their inherited values.
		 */
		if (Flag(FMONITOR) && !(flags & XXCOM)) {
			for (i = 3; --i >= 0; )
				setsig(&sigtraps[tt_sigs[i]], SIG_DFL,
					SS_RESTORE_DFL|SS_FORCE);
		}
#endif	/* JOBS && TTY_PGRP */
		if ((flags & XBGND) && !Flag(FMONITOR)) {
			setsig(&sigtraps[SIGINT], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
			setsig(&sigtraps[SIGQUIT], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
			if (!(flags & XPIPEI)) {
				i = open("/dev/null", 0);
				(void) dup2(i, 0);
				close(i);
			}
		}
		remove_job(j, "child");	/* in case of `jobs` command */
		nzombie = 0;
#ifdef JOBS
		ttypgrp_ok = 0;
		Flag(FMONITOR) = 0;
#endif /* JOBS */
		Flag(FTALKING) = 0;
		tty_close();
		cleartraps();
		execute(t, flags|XEXEC); /* no return */
		/* NOTREACHED */
	}

	/* shell (parent) stuff */
	if (!(flags&XPIPEO)) {	/* last process in a job */
#if defined(JOBS) && defined(TTY_PGRP)
		/* YYY: Is this needed? (see also YYY above)
		   if (Flag(FMONITOR) && !(flags&(XXCOM|XBGND)))
			tcsetpgrp(tty_fd, j->pgrp);
		*/
#endif	/* JOBS && TTY_PGRP */
		j_startjob(j);
		if (flags&XBGND) {
			j_set_async(j);
			if (Flag(FTALKING)) {
				shellf("[%d]", j->job);
				for (p= j->proc_list; p; p = p->next)
					shellf(" %d", p->pid);
				shellf("\n");
				shf_flush(shl_out);
			}
		} else
			rv = j_waitj(j, JW_NONE, "jw:last proc");
	}

#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */

	return rv;
}

/* start the last job: only used for `command` jobs */
void
startlast()
{
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	if (last_job) { /* no need to report error - waitlast() will do it */
		/* ensure it isn't removed by check_job() */
		last_job->flags |= JF_WAITING;
		j_startjob(last_job);
	}
#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
}

/* wait for last job: only used for `command` jobs */
int
waitlast()
{
	int	rv;
	Job	*j;
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	j = last_job;
	if (!j || !(j->flags & JF_STARTED)) {
		if (!j)
			shellf("waitlast: no last job\n");
		else
			shellf("waitlast: internal error: not started\n");
		shf_flush(shl_out);
#ifdef	JOBS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
		return 127; /* arbitrary non-zero value */
	}

	rv = j_waitj(j, JW_NONE, "jw:waitlast");

#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */

	return rv;
}

/* wait for child, interruptable. */
int
waitfor(cp)
	char	*cp;
{
	int	rv;
	Job	*j;
	int	ecode;
	int	flags = JW_INTERRUPT|JW_ASYNCNOTIFY;
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	if (cp == (char *) 0) {
		/* wait for an unspecified job - always returns 0, so
		 * don't have to worry about exited/signaled jobs
		 */
		for (j = job_list; j; j = j->next)
			/* at&t ksh will wait for stopped jobs - we don't */
			if (j->ppid == procpid && j->state == PRUNNING)
				break;
		if (!j) {
#ifdef	JOBS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
			return -1;
		}
	} else if ((j = j_lookup(cp, &ecode))) {
		/* don't report normal job completion */
		flags &= ~JW_ASYNCNOTIFY;
		if (j->ppid != procpid) {
#ifdef	JOBS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
			return -1;
		}
	} else {
#ifdef	JOBS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
		if (ecode == JL_NOSUCH)
			return -1;
		errorf("wait: %s: %s\n", cp, lookup_msgs[ecode]);
	}

	/* at&t ksh will wait for stopped jobs - we don't */
	rv = j_waitj(j, flags, "jw:waitfor");

#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */

	if (rv < 0) { /* we were interrupted */
		intrcheck();
		rv = 0; /* never reached */
	}

	return rv;
}

/* kill (built-in) a job */
int
j_kill(cp, sig)
	char	*cp;
	int	sig;
{
	Job	*j;
	Proc	*p;
	int	rv = 0;
	int	ecode;
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
		shellf("kill: %s: %s\n", cp, lookup_msgs[ecode]);
#ifdef	JOBS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
		return 1;
	}

	if (j->pgrp == 0) {	/* started when !Flag(FMONITOR) */
		for (p=j->proc_list; p != (Proc *) 0; p = p->next)
			if (kill(p->pid, sig) < 0) {
				shellf("kill: %s: %s\n", cp, strerror(errno));
				rv = 1;
			}
	} else {
#ifdef JOBS
		if (sig == SIGTERM || sig == SIGHUP)
			(void) killpg(j->pgrp, SIGCONT);
#endif
		if (killpg(j->pgrp, sig) < 0) {
			shellf("kill: %s: %s\n", cp, strerror(errno));
			rv = 1;
		}
	}

#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */

	return rv;
}

#ifdef JOBS
/* fg and bg built-ins: called only if Flag(FMONITOR) set */
int
j_resume(cp, bg)
	char	*cp;
	int	bg;
{
	Job	*j;
	Proc	*p;
	int	ecode;
	int	running;
	int	rv = 0;
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);

	if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		errorf("%s: %s: %s\n", bg ? "bg" : "fg",
			cp, lookup_msgs[ecode]);
	}

	if (j->pgrp == 0) {
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		errorf("job not job-controlled\n");
	}

	if (bg)
		shprintf("[%d] ", j->job);

	running = 0;
	for (p = j->proc_list; p != (Proc *) 0; p = p->next) {
		if (p->state == PSTOPPED) {
			p->state = PRUNNING;
			WSTATUS(p->status) = 0;
			running = 1;
		}
		shprintf("%s%s", p->command, p->next ? "| " : "");
	}
	shprintf("\n");
	shf_flush(shl_stdout);
	if (running)
		j->state = PRUNNING;

	put_job(j, PJ_PAST_STOPPED);
	if (bg)
		j_set_async(j);
	else {
# ifdef TTY_PGRP
		/* attach tty to job */
		if (j->state == PRUNNING) {
			if (ttypgrp_ok && (j->flags & JF_SAVEDTTY))
				set_tty(tty_fd, &j->ttystate, TF_NONE);
			if (ttypgrp_ok && tcsetpgrp(tty_fd, j->pgrp) < 0) {
				if (j->flags & JF_SAVEDTTY)
					set_tty(tty_fd, &tty_state, TF_NONE);
				sigprocmask(SIG_SETMASK, &omask,
					(sigset_t *) 0);
				errorf("fg: 1st tcsetpgrp(%d, %d) failed: %s\n",
					tty_fd, j->pgrp, strerror(errno));
			}
		}
# endif /* TTY_PGRP */
		j->flags |= JF_FG;
		j->flags &= ~JF_KNOWN;
		if (j == async_job)
			async_job = (Job *) 0;
	}

	if (j->state == PRUNNING && killpg(j->pgrp, SIGCONT) < 0) {
		int	err = errno;

		if (!bg) {
			j->flags &= ~JF_FG;
# ifdef TTY_PGRP
			if (ttypgrp_ok && (j->flags & JF_SAVEDTTY))
				set_tty(tty_fd, &tty_state, TF_NONE);
			if (ttypgrp_ok && tcsetpgrp(tty_fd, our_pgrp) < 0) {
				shellf("fg: 2nd tcsetpgrp(%d, %d) failed: %s\n",
					tty_fd, our_pgrp, strerror(errno));
				shf_flush(shl_out);
			}
# endif /* TTY_PGRP */
		}
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		errorf("%s: cannot continue job %s: %s\n",
			bg ? "bg" : "fg", cp, strerror(err));
	}
	if (!bg) {
# ifdef TTY_PGRP
		if (ttypgrp_ok)
			j->flags &= ~JF_SAVEDTTY;
# endif /* TTY_PGRP */
		rv = j_waitj(j, JW_NONE, "jw:resume");
	}
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
	return rv;
}
#endif	/* JOBS */

#ifdef	JOBS
/* are there any stopped jobs ? */
int
j_stopped()
{
	Job	*j;

	for (j = job_list; j != (Job *) 0; j = j->next)
		if (j->ppid == procpid && j->state == PSTOPPED) {
			shellf("You have stopped jobs\n");
			shf_flush(shl_out);
			return 1;
		}
	return 0;
}
#endif	/* JOBS */

/* list jobs for jobs built-in */
void
j_jobs(cp, slp, nflag)
	char	*cp;
	int	slp;		/* 0: short, 1: long, 2: pgrp */
	int	nflag;
{
	Job	*j, *tmp;
	int	how;
	int	zflag = 0;
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	if (nflag < 0) { /* kludge: print zombies */
		nflag = 0;
		zflag = 1;
	}
	if (cp) {
		int	ecode;

		if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
#ifdef	JOBS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
			errorf("jobs: %s: %s\n", cp, lookup_msgs[ecode]);
		}
	} else
		j = job_list;
	how = slp == 0 ? JP_MEDIUM : (slp == 1 ? JP_LONG : JP_PGRP);
	for (; j; j = j->next) {
		if ((!(j->flags & JF_ZOMBIE) || zflag)
		    && (!nflag || (j->flags & JF_CHANGED)))
		{
			j_print(j, how, shl_stdout);
			if (Flag(FMONITOR)
			    && (j->state == PEXITED || j->state == PSIGNALLED))
				j->flags |= JF_REMOVE;
		}
		if (cp)
			break;
	}
	/* Remove jobs after printing so there won't be multiple + or - jobs */
	for (j = job_list; j; j = tmp) {
		tmp = j->next;
		if (j->flags & JF_REMOVE)
			remove_job(j, "jobs");
	}
#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
}

/* list jobs for top-level notification */
void
j_notify()
{
	Job	*j, *tmp;
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */
	for (j = job_list; j; j = j->next) {
		if (Flag(FMONITOR) && (j->flags & JF_CHANGED)) {
			j_print(j, JP_MEDIUM, shl_out);
			/* Remove job after doing reports so there aren't
			 * multiple +/- jobs.
			 */
			if (j->state == PEXITED || j->state == PSIGNALLED)
				j->flags |= JF_REMOVE;
		}
	}
	for (j = job_list; j; j = tmp) {
		tmp = j->next;
		if (j->flags & JF_REMOVE)
			remove_job(j, "notify");
	}
	shf_flush(shl_out);
#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */
}

/* Return pid of last process in last asynchornous job */
pid_t
j_async()
{
#ifdef	JOBS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif	/* JOBS */

	if (async_job)
		async_job->flags |= JF_KNOWN;

#ifdef	JOBS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif	/* JOBS */

	return async_pid;
}

/* Make j the last async process
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_set_async(j)
	Job *j;
{
	Job	*jl, *oldest;

	if (async_job && (async_job->flags & (JF_KNOWN|JF_ZOMBIE)) == JF_ZOMBIE)
		remove_job(async_job, "async");
	if (!(j->flags & JF_STARTED)) {
		shellf("j_async: job not started!\n");
		shf_flush(shl_out);
		return;
	}
	async_job = j;
	async_pid = j->last_proc->pid;
	while (nzombie > child_max) {
		oldest = (Job *) 0;
		for (jl = job_list; jl; jl = jl->next)
			if (jl != async_job && (jl->flags & JF_ZOMBIE)
			    && (!oldest || jl->age < oldest->age))
				oldest = jl;
		if (!oldest) {
			/* XXX debugging */
			if (!(async_job->flags & JF_ZOMBIE) || nzombie != 1) {
				shellf("j_async: bad nzombie (%d)\n", nzombie);
				shf_flush(shl_out);
				nzombie = 0;
			}
			break;
		}
		remove_job(oldest, "zombie");
	}
}

/* Start a job: set STARTED, check for held signals and set j->last_proc
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_startjob(j)
	Job *j;
{
	Proc	*p;

	j->flags |= JF_STARTED;
	for (p = j->proc_list; p->next; p = p->next)
		;
	j->last_proc = p;

#ifdef	JOBS
# ifdef NEED_PGRP_SYNC
	if (j_sync_open) {
		closepipe(j_sync_pipe);
		j_sync_open = 0;
	}
# endif /* NEED_PGRP_SYNC */
	if (held_sigchld) {
		held_sigchld = 0;
		/* Don't call j_sigchild() as it may remove job... */
		kill(procpid, SIGCHLD);
	}
#endif	/* JOBS */
}

/*
 * wait for job to complete or change state
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static int
j_waitj(j, flags, where)
	Job	*j;
	int	flags;		/* see JW_* */
	char	*where;
{
	int	rv;

	/*
	 * No auto-notify on the job we are waiting on.
	 */
	j->flags |= JF_WAITING;
	if (flags & JW_ASYNCNOTIFY)
		j->flags |= JF_W_ASYNCNOTIFY;

	if (!Flag(FMONITOR))
		flags |= JW_STOPPEDWAIT;

	while ((volatile int) j->state == PRUNNING
		|| ((flags & JW_STOPPEDWAIT)
		    && (volatile int) j->state == PSTOPPED))
	{
#ifdef	JOBS
		sigsuspend(&sm_default);
#else	/* JOBS */
		j_sigchld(SIGCHLD);
#endif	/* JOBS */
		if ((flags & JW_INTERRUPT) && intrsig) {
			j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);
			return -1;
		}
	}
	j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);

	if (j->flags & JF_FG) {
		WAIT_T	status;

		j->flags &= ~JF_FG;
#if defined(JOBS) && defined(TTY_PGRP)
		if (Flag(FMONITOR) && ttypgrp_ok && j->pgrp) {
			if (tcsetpgrp(tty_fd, our_pgrp) < 0) {
				shellf(
				    "j_waitj: tcsetpgrp(%d, %d) failed: %s\n",
					tty_fd, our_pgrp, strerror(errno));
				shf_flush(shl_out);
			}
			if (j->state == PSTOPPED) {
				j->flags |= JF_SAVEDTTY;
				get_tty(tty_fd, &j->ttystate);
			}
		}
#endif	/* JOBS && TTY_PGRP */
		if (tty_fd >= 0) {
			if (j->state == PEXITED && j->status == 0)
				get_tty(tty_fd, &tty_state);
			else
				set_tty(tty_fd, &tty_state,
				    (j->state == PEXITED) ? 0 : TF_MIPSKLUDGE);
		}
		/* If it looks like user hit ^C to kill a job, pretend we got
		 * one too to break out of for loops, etc.  (at&t ksh does this
		 * even when not monitoring, but this doesn't make sense since
		 * a tty generated ^C goes to the whole process group)
		 */
		status = j->last_proc->status;
		if (Flag(FMONITOR) && j->state == PSIGNALLED
		    && WIFSIGNALED(status)
		    && (sigtraps[WTERMSIG(status)].flags & TF_TTY_INTR))
			trapsig(WTERMSIG(status));
	}

	j_utime = j->utime;
	j_stime = j->stime;
	rv = j->status;

	if (!(flags & JW_ASYNCNOTIFY) 
	    && (!Flag(FMONITOR) || j->state != PSTOPPED))
	{
		j_print(j, JP_SHORT, shl_out);
		shf_flush(shl_out);
	}
	if (j->state != PSTOPPED
	    && (!Flag(FMONITOR) || !(flags & JW_ASYNCNOTIFY)))
		remove_job(j, where);

	return rv;
}

/* SIGCHLD handler to reap children and update job states
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static RETSIGTYPE
j_sigchld(sig)
	int	sig;
{
	int		errno_ = errno;
	Job		*j;
	Proc		UNINITIALIZED(*p);
	int		pid;
	WAIT_T		status;
	struct tms	t0, t1;

	trapsig(sig);

#ifdef	JOBS
	/* Don't wait for any processes if a job is partially started.
	 * This is so we don't do away with the process group leader
	 * before all the processes in a pipe line are started (so the
	 * setpgrp() won't fail)
	 */
	for (j = job_list; j; j = j->next)
		if (j->ppid == procpid && !(j->flags & JF_STARTED)) {
			held_sigchld = 1;
			return;
		}
#endif	/* JOBS */

	ksh_times(&t0);
	do {
#ifdef	JOBS
		pid = waitpid(-1, &status, (WNOHANG|WUNTRACED));
#else	/* JOBS */
		pid = wait(&status);
#endif	/* JOBS */

		if (pid <= 0)	/* return if would block (0) ... */
			break;	/* ... or no children or interrupted (-1) */

		ksh_times(&t1);

		/* find job and process structures for this pid */
		for (j = job_list; j != (Job *) 0; j = j->next)
			for (p = j->proc_list; p != (Proc *) 0; p = p->next)
				if (p->pid == pid)
					goto found;
found:
		if (j == (Job *) 0) {
			/* Can occur if process has kids, then execs shell
			shellf("bad process waited for (pid = %d)\n", pid);
			shf_flush(shl_out);
			 */
			t0 = t1;
			continue;
		}

		j->utime += t1.tms_cutime - t0.tms_cutime;
		j->stime += t1.tms_cstime - t0.tms_cstime;
		t0 = t1;
		p->status = status;
#ifdef	JOBS
		if (WIFSTOPPED(status))
			p->state = PSTOPPED;
		else
#endif	/* JOBS */
		if (WIFSIGNALED(status))
			p->state = PSIGNALLED;
		else
			p->state = PEXITED;

		check_job(j);	/* check to see if entire job is done */
	}
#ifdef	JOBS
	while (1);
#else	/* JOBS */
	while (0);
#endif	/* JOBS */

	errno = errno_;
}

/*
 * Called only when a process in j has exited/stopped (ie, called only
 * from j_sigchild()).  If no processes are running, the job status
 * and state are updated, asynchronous job notification is done and,
 * if unneeded, the job is removed.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
check_job(j)
	Job	*j;
{
	int	jstate;
	Proc	*p;

	/* XXX debugging (nasty - interrupt routine using shl_out) */
	if (!(j->flags & JF_STARTED)) {
		shellf("check_job: called on started job (flags 0x%x)\n",
			j->flags);
		shf_flush(shl_out);
		return;
	}

	jstate = PRUNNING;
	for (p=j->proc_list; p != (Proc *) 0; p = p->next) {
		if (p->state == PRUNNING)
			return;	/* some processes still running */
		if (p->state > jstate)
			jstate = p->state;
	}
	j->state = jstate;

	switch (j->last_proc->state) {
	case PEXITED:
		j->status = WEXITSTATUS(j->last_proc->status);
		break;
	case PSIGNALLED:
		j->status = 128 + WTERMSIG(j->last_proc->status);
		break;
	default:
		j->status = 0;
		break;
	}

	j->flags |= JF_CHANGED;
	if (Flag(FMONITOR) && !(j->flags & JF_XXCOM)) {
		/* Only put stopped jobs at the front to avoid confusing
		 * the user (don't want finished jobs effecting %+ or %-)
		 */
		if (j->state == PSTOPPED)
			put_job(j, PJ_ON_FRONT);
		if (Flag(FNOTIFY)
		    && (j->flags & (JF_WAITING|JF_W_ASYNCNOTIFY)) != JF_WAITING)
		{
			/* Look for the real file descriptor 2 */
			{
				struct env *ep;
				int fd = 2;

				for (ep = e; ep; ep = ep->oenv)
					if (ep->savefd && ep->savefd[2])
						fd = ep->savefd[2];
				shf_reopen(fd, SHF_WR, shl_j);
			}
			/* Can't call j_notify() as it removes jobs.  The job
			 * must stay in the job list as j_waitj() may be
			 * running with this job.
			 */
			j_print(j, JP_MEDIUM, shl_j);
			shf_flush(shl_j);
			if (!(j->flags & JF_WAITING) && j->state != PSTOPPED)
				remove_job(j, "notify");
		}
	}
	if (!Flag(FMONITOR) && !(j->flags & (JF_WAITING|JF_FG))
	    && j->state != PSTOPPED)
	{
		if (j == async_job || (j->flags & JF_KNOWN)) {
			j->flags |= JF_ZOMBIE;
			j->job = -1;
			nzombie++;
		} else
			remove_job(j, "checkjob");
	}
}

/*
 * Print job status in either short, medium or long format.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_print(j, how, shf)
	Job		*j;
	int		how;
	struct shf	*shf;
{
	Proc	*p;
	int	state;
	WAIT_T	status;
	int	coredumped;
	char	jobchar = ' ';
	char	buf[64], *filler;
	int	output = 0;

	if (how == JP_PGRP) {
		/* POSIX doesn't say what to do it there is no process
		 * group leader (ie, !FMONITOR).  We arbitrarily return
		 * last pid (which is what $! returns).
		 */
		shf_fprintf(shf, "%d\n", j->pgrp ? j->pgrp
				: (j->last_proc ? j->last_proc->pid : 0));
		return;
	}
	j->flags &= ~JF_CHANGED;
	filler = j->job > 10 ?  "\n       " : "\n      ";
	if (j == job_list)
		jobchar = '+';
	else if (j == job_list->next)
		jobchar = '-';

	for (p = j->proc_list; p != (Proc *) 0;) {
		coredumped = 0;
		switch (p->state) {
		case PRUNNING:
			strcpy(buf, "Running");
			break;
		case PSTOPPED:
			strcpy(buf, sigtraps[WSTOPSIG(p->status)].mess);
			break;
		case PEXITED:
			if (how == JP_SHORT)
				buf[0] = '\0';
			else if (WEXITSTATUS(p->status) == 0)
				strcpy(buf, "Done");
			else
				shf_snprintf(buf, sizeof(buf), "Done (%d)",
					WEXITSTATUS(p->status));
			break;
		case PSIGNALLED:
			if (WIFCORED(p->status))
				coredumped = 1;
			/* kludge for not reporting `normal termination signals'
			 * (ie, SIGINT, SIGPIPE)
			 */
			if (how == JP_SHORT && !coredumped
			    && (WTERMSIG(p->status) == SIGINT
				|| WTERMSIG(p->status) == SIGPIPE)) {
				buf[0] = '\0';
			} else
				strcpy(buf, sigtraps[WTERMSIG(p->status)].mess);
			break;
		}

		if (how != JP_SHORT)
			if (p == j->proc_list)
				shf_fprintf(shf, "[%d] %c ", j->job, jobchar);
			else
				shf_fprintf(shf, "%s", filler);

		if (how == JP_LONG)
			shf_fprintf(shf, "%5d ", p->pid);

		if (how == JP_SHORT) {
			if (buf[0]) {
				output = 1;
				shf_fprintf(shf, "%s%s ",
					buf, coredumped ? " (core dumped)" : "");
			}
		} else {
			output = 1;
			shf_fprintf(shf, "%-20s %s%s%s", buf, p->command,
				p->next ? "|" : "",
				coredumped ? " (core dumped)" : "");
		}

		state = p->state;
		status = p->status;
		p = p->next;
		while (p && p->state == state
		       && WSTATUS(p->status) == WSTATUS(status))
		{
			if (how == JP_LONG)
				shf_fprintf(shf, "%s%5d %-20s %s%s", filler, p->pid,
					" ", p->command, p->next ? "|" : "");
			else if (how == JP_MEDIUM)
				shf_fprintf(shf, " %s%s", p->command,
					p->next ? "|" : "");
			p = p->next;
		}
	}
	if (output)
		shf_fprintf(shf, "\n");
}

/* Convert % sequence to job
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
j_lookup(cp, ecodep)
	char	*cp;
	int	*ecodep;
{
	Job		*j, *last_match;
	Proc		*p;
	int		len, job = 0;

	if (digit(*cp)) {
		job = atoi(cp);
		/* Look for last_proc->pid (what $! returns) first... */
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->last_proc && j->last_proc->pid == job)
				return j;
		/* ...then look for process group (this is non-POSIX),
		 * but should not break anything (so FPOSIX isn't used).
		 */
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->pgrp && j->pgrp == job)
				return j;
		if (ecodep)
			*ecodep = JL_NOSUCH;
		return (Job *) 0;
	}
	if (*cp != '%') {
		if (ecodep)
			*ecodep = JL_INVALID;
		return (Job *) 0;
	}
	switch (*++cp) {
	  case '\0': /* non-standard */
	  case '+':
	  case '%':
		if (job_list != (Job *) 0)
			return job_list;
		break;

	  case '-':
		if (job_list != (Job *) 0 && job_list->next)
			return job_list->next;
		break;

	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
		job = atoi(cp);
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->job == job)
				return j;
		break;

	  case '?':		/* %?string */
		last_match = (Job *) 0;
		for (j = job_list; j != (Job *) 0; j = j->next)
			for (p = j->proc_list; p != (Proc *) 0; p = p->next)
				if (strstr(p->command, cp+1) != (char *) 0) {
					if (last_match) {
						if (ecodep)
							*ecodep = JL_AMBIG;
						return (Job *) 0;
					}
					last_match = j;
				}
		if (last_match)
			return last_match;
		break;

	  default:		/* %string */
		len = strlen(cp);
		last_match = (Job *) 0;
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (strncmp(cp, j->proc_list->command, len) == 0) {
				if (last_match) {
					if (ecodep)
						*ecodep = JL_AMBIG;
					return (Job *) 0;
				}
				last_match = j;
			}
		if (last_match)
			return last_match;
		break;
	}
	if (ecodep)
		*ecodep = JL_NOSUCH;
	return (Job *) 0;
}

static Job	*free_jobs = (Job *) 0;
static Proc	*free_procs = (Proc *) 0;

/* allocate a new job and fill in the job number.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
new_job()
{
	int	i;
	Job	*newj, *j;

	if (free_jobs != (Job *) 0) {
		newj = free_jobs;
		free_jobs = free_jobs->next;
	} else
		newj = (Job *) alloc(sizeof(Job), APERM);

	/* brute force method */
	for (i = 1; ; i++) {
		for (j = job_list; j && j->job != i; j = j->next)
			;
		if (j == (Job *) 0)
			break;
	}
	newj->job = i;

	return newj;
}

/* Allocate new process strut
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Proc *
new_proc()
{
	Proc	*p;

	if (free_procs != (Proc *) 0) {
		p = free_procs;
		free_procs = free_procs->next;
	} else
		p = (Proc *) alloc(sizeof(Proc), APERM);

	return p;
}

/* Take job out of job_list and put old structures into free list.
 * Keeps nzombies, last_job and async_job up to date.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
remove_job(j, where)
	Job	*j;
	char	*where;
{
	Proc	*p, *tmp;
	Job	**prev, *curr;

	prev = &job_list;
	curr = *prev;
	for (; curr != (Job *) 0 && curr != j; prev = &curr->next, curr = *prev)
		;
	if (curr == j) {
		*prev = curr->next;
	} else {
		shellf("remove_job: job not found (%s)\n", where);
		shf_flush(shl_out);
		return;
	}

	/* free up proc structures */
	for (p = j->proc_list; p != (Proc *) 0; ) {
		tmp = p;
		p = p->next;
		tmp->next = free_procs;
		free_procs = tmp;
	}

	if ((j->flags & JF_ZOMBIE) && j->ppid == procpid)
		--nzombie;
	j->next = free_jobs;
	free_jobs = j;

	if (j == last_job)
		last_job = (Job *) 0;
	if (j == async_job)
		async_job = (Job *) 0;
}

/* put j in a particular location (taking it out job_list if it is there
 * already)
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
put_job(j, where)
	Job	*j;
	int	where;
{
	Job	**prev, *curr;

	/* Remove job from list (if there) */
	prev = &job_list;
	curr = job_list;
	for (; curr && curr != j; prev = &curr->next, curr = *prev)
		;
	if (curr == j)
		*prev = curr->next;

	switch (where) {
	case PJ_ON_FRONT:
		j->next = job_list;
		job_list = j;
		break;

	case PJ_PAST_STOPPED:
		prev = &job_list;
		curr = job_list;
		for (; curr && curr->state == PSTOPPED; prev = &curr->next,
							curr = *prev)
			;
		j->next = curr;
		*prev = j;
		break;
	}
}

/* nuke a job (called when unable to start full job).
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
kill_job(j)
	Job	*j;
{
	Proc	*p;

	for (p = j->proc_list; p != (Proc *) 0; p = p->next)
		if (p->pid != 0)
			(void) kill(p->pid, 9);
}

/* put a more useful name on a process than snptreef does (in certain cases) */
static void
fill_command(c, len, t)
	char		*c;
	int		len;
	struct op	*t;
{
	int		alen;
	char		**ap;
	extern char	**eval();

	if (t->type == TEXEC || t->type == TCOM) {
		if (t->type == TCOM)
			ap = eval(t->args, DOBLANK|DONTRUNCOMMAND);
		else
			ap = t->args;
		--len; /* save room for the null */
		while (len > 0 && *ap != (char *) 0) {
			alen = strlen(*ap);
			if (alen > len)
				alen = len;
			memcpy(c, *ap, alen);
			c += alen;
			len -= alen;
			if (len > 0) {
				*c++ = ' '; len--;
			}
			ap++;
		}
		*c = '\0';
	} else
		snptreef(c, len, "%T", t);
}
