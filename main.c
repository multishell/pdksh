/*
 * startup, main loop, enviroments and error handling
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: main.c,v 1.5 1994/05/31 13:34:34 michael Exp $";
#endif

#define	EXTERN				/* define EXTERNs in sh.h */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

/*
 * global data
 */

static	void	reclaim ARGS((void));
static	void	remove_temps ARGS((struct temp *tp));

/*
 * shell initialization
 */

static	char	initifs [] = "IFS= \t\n"; /* must be R/W */

static	const	char   initsubs [] = 
  "${SHELL=/bin/sh} ${PS1=$ } ${PS2=> } ${PS3=#? }";

static	const	char *initcoms [] = {
	"typeset", "-x", "SHELL", "PATH", "HOME", NULL,
	"typeset", "-r", "KSH_VERSION", "PWD", "OLDPWD", NULL,
	"typeset", "-ri", "PPID", NULL,
	"typeset", "-i", "SECONDS=0", "OPTIND=1", "RANDOM", "MAILCHECK=600",
	  "TMOUT=0", NULL,
	"alias",
	 /* Standard ksh aliases */
	  "autoload=typeset -fu",
	  "functions=typeset -f",
	  "hash=alias -t -",
	  "history=fc -l",
	  "integer=typeset -i",
	  "nohup=nohup ",
	  "r=fc -e -",
#ifdef JOBS
	  "stop=kill -STOP",
	  "suspend=kill -STOP $$",
#endif
	  "type=whence -v",
	 /* Aliases that are builtin commands in at&t */
	  "pwd=print -r - \"$PWD\"",
	  "login=exec login",
	  "newgrp=exec newgrp",
	  NULL,
	/* this is what at&t ksh seems to track, with the addition of emacs */
	"alias", "-tU",
	  "cat", "cc", "chmod", "cp", "date", "ed", "emacs", "grep", "ls",
	  "mail", "make", "mv", "pr", "rm", "sed", "sh", "vi", "who",
	  NULL,
	NULL
};

int
main(argc, argv, envp)
	int argc;
	register char **argv;
	char **envp;
{
	register int i;
	int pwd_allocated;
	char *arg;
	int argi;
	char *name;
	Source *s;
	struct block *l;
	char **wp;
	struct stat s1, s2;
	extern char ksh_version [];
	struct env env;

#ifdef MEM_DEBUG
	chmem_push("+c", 1);
	/*chmem_push("+cd", 1);*/
#endif

	/* make sure argv[] is sane */
	if (!*argv) {
		static	char	*empty_argv[] = { "pdksh", (char *) 0 };

		argv = empty_argv;
		argc = 1;
	}
	name = *argv;

	ainit(&aperm);		/* initialize permanent Area */

	/* set up base enviroment */
	env.type = E_NONE;
	ainit(&env.area);
	env.savefd = NULL;
	env.oenv = NULL;
	env.loc = (struct block *) 0;
	e = &env;
	newblock();		/* set up global l->vars and l->funs */

	initctypes();

	inittraps();

	initio();

	/* set up variable and command dictionaries */
	tinit(&taliases, APERM);
	tinit(&builtins, APERM);
	tinit(&aliases, APERM);
	tinit(&keywords, APERM);
	tinit(&homedirs, APERM);

	/* define shell keywords */
	initkeywords();

	/* define built-in commands */
	for (i = 0; shbuiltins[i].name != NULL; i++)
		builtin(shbuiltins[i].name, shbuiltins[i].func);
	for (i = 0; kshbuiltins[i].name != NULL; i++)
		builtin(kshbuiltins[i].name, kshbuiltins[i].func);

	init_histvec();

	def_path = DEFAULT__PATH;
#if defined(HAVE_CONFSTR) && defined(_CS_PATH)
	{
		size_t len = confstr(_CS_PATH, (char *) 0, 0);

		if (len > 0) {
			def_path = alloc(len + 1, APERM);
			confstr(_CS_PATH, def_path, len + 1);
		}
	}
#endif /* HAVE_CONFSTR && _CS_PATH */
	path = def_path;

	/* set posix flag just before environment so that it will have
	 * exactly the same effect as the POSIXLY_CORRECT environment
	 * variable.  If this needs to be done sooner to ensure correct posix
	 * operation, an initial scan of the environment will also have
	 * done sooner.
	 */
#ifdef POSIXLY_CORRECT
	Flag(FPOSIX) = 1;
#endif /* POSIXLY_CORRECT */

	/* import enviroment */
	if (envp != NULL)
		for (wp = envp; *wp != NULL; wp++)
			import(*wp);

	kshpid = procpid = getpid();
	typeset(initifs, 0, 0, 0, 0);	/* for security */

	/* assign default shell variable values */
	substitute(initsubs, 0);

	/* assign PWD (using enviroment PWD if it passes test below) */
	arg = strval(global("PWD"));
	pwd_allocated = 0;
	if (*arg == '\0'
		|| stat(arg, &s1) < 0 || stat(".", &s2) < 0
		|| s1.st_dev != s2.st_dev || s1.st_ino != s2.st_ino)
	{
		/* Ignore imported PWD - its wrong */
		if ((arg = alloc(PATH, ATEMP)) == (char *) 0
			|| getcwd(arg, PATH) == (char *) 0)
		{
			shellf(
		    "%s: Can't find current directory (%s), changing to /\n",
				name, strerror(errno));
			shf_flush(shl_out);
			if (arg)
				afree(arg, ATEMP);
			arg = "/";
			(void) chdir(arg);
		} else
			pwd_allocated = 1;
	}
	setstr(global("PWD"), arg);
	if (pwd_allocated)
		afree(arg, ATEMP);
	setint(global("PPID"), (long) getppid());
	setint(global("RANDOM"), (long) time((time_t *)0));
	setstr(global("KSH_VERSION"), ksh_version);

	/* execute initialization statements */
	for (wp = (char**) initcoms; *wp != NULL; wp++) {
		shcomexec(wp);
		for (; *wp != NULL; wp++)
			;
	}

	if (geteuid() == 0)
		setstr(global("PS1"), "# ");

	s = pushs(SFILE);

	/* Set this before parsing arguments */
	Flag(FPRIVILEGED) = getuid() != geteuid() || getgid() != getegid();

	/* this to note if monitor is set on command line (see below) */
	Flag(FMONITOR) = 127;

	argi = parse_args(argv, OF_CMDLINE, &s->str, (int *) 0);

	if (Flag(FCOMMAND)) {
		s->type = SSTRING;
		if (argv[argi])
			name = argv[argi++];
	} else if (argi < argc && !Flag(FSTDIN)) {
		s->file = name = argv[argi++];
		s->u.shf = shf_open(name, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
		if (s->u.shf == NULL)
			errorf("%s: cannot open\n", name);
	} else {
		Flag(FSTDIN) = 1;
		if (isatty(0) && isatty(2))
			Flag(FTALKING) = 1;
		if (Flag(FTALKING)) {
			s->type = STTY;
#ifdef EDIT
			x_init();
#endif
		} else
			s->u.shf = shf_fdopen(0, SHF_RD | can_seek(0),
				(struct shf *) 0);
	}

	/* initialize job control */
	i = Flag(FMONITOR) != 127;
	Flag(FMONITOR) = 0;
	j_init(i);

	l = e->loc;
	l->argv = &argv[argi - 1];
	l->argc = argc - argi;
	l->argv[0] = name;
	getopts_reset(1);

	if (name[0] == '-' || ((arg = strrchr(name, '/')) && *++arg == '-')) {
		(void) include("/etc/profile", 0, (char **) 0);
		if (!Flag(FPRIVILEGED))
			(void) include(substitute("$HOME/.profile", 0), 0,
				(char **) 0);
	}

	if (Flag(FPRIVILEGED))
		(void) include("/etc/suid_profile", 0, (char **) 0);
	else {
		/* include $ENV */
		arg = substitute(strval(global("ENV")), DOTILDE);
		if (*arg != '\0')
			(void) include(arg, 0, (char **) 0);
	}

	if (Flag(FTALKING)) {
		hist_init(s);
		alarm_init();
	} else
		Flag(FTRACKALL) = 1;	/* set after ENV */

	shell(s, TRUE);	/* doesn't return */
	return 0;
}

int
include(name, argc, argv)
	register char *name;
	int argc;
	char **argv;
{
	register Source *volatile s = NULL;
	struct shf *shf;
	char **volatile old_argv;
	volatile int old_argc;
	int i;

	shf = shf_open(name, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
	if (shf == NULL)
		return 0;

	if (argv) {
		old_argv = e->loc->argv;
		old_argc = e->loc->argc;
	} else {
		old_argv = (char **) 0;
		old_argc = 0;
	}
	newenv(E_INCL);
	if ((i = setjmp(e->jbuf))) {
		quitenv();
		if (s)
			shf_close(s->u.shf);
		if (old_argv) {
			e->loc->argv = old_argv;
			e->loc->argc = old_argc;
		}
		switch (i) {
		  case LRETURN:
		  case LERROR:
			return 1;
		  case LINTR:
		  case LEXIT:
		  case LLEAVE:
		  case LSHELL:
			unwind(i);
			/*NOREACHED*/
		  default:
			errorf("internal error: E_INCL %d\n", i);
			/*NOREACHED*/
		}
	}
	if (argv) {
		e->loc->argv = argv;
		e->loc->argc = argc;
	}
	s = pushs(SFILE);
	s->u.shf = shf;
	s->file = strsave(name, ATEMP);
	shell(s, FALSE);
	quitenv();
	shf_close(s->u.shf);
	if (old_argv) {
		e->loc->argv = old_argv;
		e->loc->argc = old_argc;
	}
	return 1;
}

int
command(comm)
	register char *comm;
{
	register Source *s;

	s = pushs(SSTRING);
	s->str = comm;
	return shell(s, FALSE);
}

/*
 * run the commands from the input source, returning status.
 */
int
shell(s, exit_atend)
	Source *volatile s;		/* input source */
	int volatile exit_atend;
{
	struct op *t;
	int wastty;
	volatile int attempts = 13;
#ifdef JOBS
	volatile int interactive = Flag(FTALKING) && s->type == STTY;
#endif /* JOBS */
	int i;

	newenv(E_PARSE);
	exstat = 0;
#ifdef JOBS
	if (interactive)
		really_exit = 0;
#endif /* JOBS */
	if ((i = setjmp(e->jbuf))) {
		s->str = null;
		switch (i) {
		  case LSHELL:
			if (interactive) {
				/* Used by exit command to get back to
				 * top level shell.  Kind of strange since
				 * interactive is set if we are reading from
				 * a tty, but to have stopped jobs, one only
				 * needs FMONITOR set (not FTALKING/STTY)...
				 */
				break;
			}
			/* fall through... */
		  case LEXIT:
		  case LLEAVE:
		  case LRETURN:
			quitenv();
			unwind(i);	/* keep on going */
			/*NOREACHED*/
		  case LERROR:
			break;
		  case LINTR:
			/* we get here if SIGINT not caught or ignored */
			shellf("\n");
			break;
		  default:
			errorf("internal error: E_PARSE %d\n", i);
			/*NOREACHED*/
		}
	}

	while (1) {
		if (trap)
			runtraps(FALSE);

		if (s->next == NULL)
			if (Flag(FVERBOSE))
				s->flags |= SF_ECHO;
			else
				s->flags &= ~SF_ECHO;

		j_notify();

		if ((wastty = (s->type == STTY)) || s->type == SHIST) {
			set_prompt(PS1);
			mcheck();
		}

		t = compile(s);
		if (t != NULL && t->type == TEOF) {
			if (wastty && Flag(FIGNOREEOF) && --attempts > 0) {
				shellf("Use `exit' to leave ksh\n");
				s->type = STTY;
#ifdef	JOBS
			} else if (wastty && !really_exit && j_stopped()) {
				really_exit = 1;
				s->type = STTY;
#endif	/* JOBS */
			} else {
				/* this for POSIX, which says EXIT traps
				 * shell be taken in the environment
				 * immediately after the last command
				 * executed.
				 */
				if (exit_atend)
					unwind(LEXIT);
				break;
			}
		}
		/* no longer needed? shf_flush(shl_out); flush -v output */

		if (!Flag(FNOEXEC) || s->type == STTY)
			exstat = execute(t, 0);

#ifdef JOBS
		if (t != NULL && t->type != TEOF && interactive && really_exit)
			really_exit = 0;
#endif	/* JOBS */

		reclaim();
	}
	quitenv();
	return exstat;
}

/* return to closest error handler or shell(), exit if none found */
void
unwind(i)
	int i;
{
	/* ordering for EXIT vs ERR is a bit odd (this is what at&t ksh does) */
	if (i == LEXIT || (Flag(FERREXIT) && (i == LERROR || i == LINTR)
			   && sigtraps[SIGEXIT_].trap))
	{
		runtrap(&sigtraps[SIGEXIT_]);
		i = LLEAVE;
	} else if (Flag(FERREXIT) && (i == LERROR || i == LINTR)) {
		runtrap(&sigtraps[SIGERR_]);
		i = LLEAVE;
	}
	while (1) {
		switch (e->type) {
		  case E_PARSE:
		  case E_FUNC:
		  case E_INCL:
		  case E_LOOP:
		  case E_ERRH:
			longjmp(e->jbuf, i);
			/*NOTREACHED*/

		  case E_NONE: 	/* bottom of the stack */
		  {
			if (Flag(FTALKING))
				hist_finish();
			j_exit();
			remove_temps(func_heredocs);
			if (i == LINTR) {
				int sig = exstat - 128;

				/* ham up our death a bit (at&t ksh
				 * only seems to do this for SIGTERM)
				 * Don't do it for SIGQUIT, since we'd
				 * dump a core..
				 */
				if (sig == SIGINT || sig == SIGTERM) {
					setsig(&sigtraps[sig], SIG_DFL,
						SS_RESTORE_CURR|SS_FORCE);
					kill(0, sig);
				}
			}
			exit(exstat);
			/* NOTREACHED */
		  }

		  default:
			quitenv();
		}
	}
}

void
newenv(type)
	int type;
{
	register struct env *ep;

	ep = (struct env *) alloc(sizeof(*ep), ATEMP);
	ep->type = type;
	ep->flags = 0;
	ainit(&ep->area);
	ep->loc = e->loc;
	ep->savefd = NULL;
	ep->oenv = e;
	ep->temps = NULL;
	e = ep;
}

void
quitenv()
{
	register struct env *ep = e;
	register int fd;

	if (ep->oenv == NULL)
		exit(exstat);	/* exit child */
	if (ep->oenv->loc != ep->loc)
		popblock();
	if (ep->savefd != NULL) {
		for (fd = 0; fd < NUFILE; fd++)
			if (ep->savefd[fd] > 0)
				restfd(fd, ep->savefd[fd]);
		if (ep->savefd[2]) /* Clear any write errors */
			shf_reopen(2, SHF_WR, shl_out);
	}
	reclaim();
	e = e->oenv;
	afree(ep, ATEMP);
}

/* remove temp files and free ATEMP Area */
static void
reclaim()
{
	remove_temps(e->temps);
	e->temps = NULL;
	afreeall(&e->area);
}

static void
remove_temps(tp)
	struct temp *tp;
{
	for (; tp != NULL; tp = tp->next)
		if (tp->pid == procpid)
			unlink(tp->name);
}

void
aerror(ap, msg)
	Area *ap;
	const char *msg;
{
	errorf("alloc internal error: %s\n", msg);
}

