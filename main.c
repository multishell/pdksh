/*
 * startup, main loop, enviroments and error handling
 */

#define	EXTERN				/* define EXTERNs in sh.h */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

/*
 * global data
 */

static	void	reclaim ARGS((void));
static	void	remove_temps ARGS((struct temp *tp));
static	int	is_restricted ARGS((char *name));

/*
 * shell initialization
 */

static	char	initifs [] = "IFS= \t\n"; /* must be R/W */

static	const	char   initsubs [] = 
  "${PS2=> } ${PS3=#? } ${PS4=+ }";

static const char version_param[] =
#ifdef KSH
	"KSH_VERSION"
#else /* KSH */
	"SH_VERSION"
#endif /* KSH */
	;

static	const	char *initcoms [] = {
	"typeset", "-x", "SHELL", "PATH", "HOME", NULL,
	"typeset", "-r", version_param, NULL,
	"typeset", "-ri", "PPID", NULL,
	"typeset", "-i", "OPTIND=1", "MAILCHECK=600",
#ifdef KSH
	    "SECONDS=0", "RANDOM", "TMOUT=0",
#endif /* KSH */
	    NULL,
	"alias",
	 /* Standard ksh aliases */
	  "hash=alias -t --",
#ifdef JOBS
	  "stop=kill -STOP",
	  "suspend=kill -STOP $$",
#endif
#ifdef KSH
	  "autoload=typeset -fu",
	  "functions=typeset -f",
	  "history=fc -l",
	  "integer=typeset -i",
	  "nohup=nohup ",
	  "local=typeset",
	  "r=fc -e -",
	  "type=whence -v",
#endif /* KSH */
#ifdef KSH
	 /* Aliases that are builtin commands in at&t */
	  "login=exec login",
	  "newgrp=exec newgrp",
#endif /* KSH */
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
	int argi;
	Source *s;
	struct block *l;
	int restricted;
	char **wp;
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
	kshname = *argv;

	ainit(&aperm);		/* initialize permanent Area */

	/* set up base enviroment */
	env.type = E_NONE;
	ainit(&env.area);
	env.savefd = NULL;
	env.oenv = NULL;
	env.loc = (struct block *) 0;
	e = &env;
	newblock();		/* set up global l->vars and l->funs */

	/* Do this first so output routines (eg, errorf, shellf) can work */
	initio();

	initvar();

	initctypes();

	inittraps();

	coproc_init();

	/* set up variable and command dictionaries */
	tinit(&taliases, APERM, 0);
	tinit(&aliases, APERM, 0);
	tinit(&homedirs, APERM, 0);

	/* define shell keywords */
	initkeywords();

	/* define built-in commands */
	tinit(&builtins, APERM, 64); /* must be 2^n (currently 40 builtins) */
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


	/* Turn on nohup by default for how - will change to off
	 * by default once people are aware of its existance
	 * (at&t ksh does not have a nohup option - it always sends
	 * the hup).
	 */
	Flag(FNOHUP) = 1;

	/* Turn on brace expansion by default.  At&t ksh's that have
	 * alternation always have it on.  BUT, posix doesn't have
	 * brace expansion, so set this before setting up FPOSIX
	 * (change_flag() clears FBRACEEXPAND when FPOSIX is set).
	 */
#ifdef BRACEEXPAND
	Flag(FBRACEEXPAND) = 1;
#endif /* BRACEEXPAND */

	/* set posix flag just before environment so that it will have
	 * exactly the same effect as the POSIXLY_CORRECT environment
	 * variable.  If this needs to be done sooner to ensure correct posix
	 * operation, an initial scan of the environment will also have
	 * done sooner.
	 */
#ifdef POSIXLY_CORRECT
	change_flag(FPOSIX, OF_SPECIAL, 1);
#endif /* POSIXLY_CORRECT */

	/* import enviroment */
	if (envp != NULL)
		for (wp = envp; *wp != NULL; wp++)
			typeset(*wp, IMPORT|EXPORT, 0, 0, 0);

	kshpid = procpid = getpid();
	typeset(initifs, 0, 0, 0, 0);	/* for security */

	/* assign default shell variable values */
	substitute(initsubs, 0);

	/* Figure out the current working directory and set $PWD */
	{
		struct stat s_pwd, s_dot;
		struct tbl *pwd_v = global("PWD");
		char *pwd = strval(pwd_v);
		char *pwdx = pwd;

		/* Try to use existing $PWD if it is valid */
		if (!ISABSPATH(pwd)
		    || stat(pwd, &s_pwd) < 0 || stat(".", &s_dot) < 0
		    || s_pwd.st_dev != s_dot.st_dev
		    || s_pwd.st_ino != s_dot.st_ino)
			pwdx = (char *) 0;
		set_current_wd(pwdx);
		if (current_wd[0])
			simplify_path(current_wd);
		/* Only set pwd if we know where we are or if it had a
		 * bogus value
		 */
		if (current_wd[0] || pwd != null)
			setstr(pwd_v, current_wd);
	}
	setint(global("PPID"), (long) getppid());
#ifdef KSH
	setint(global("RANDOM"), (long) time((time_t *)0));
#endif /* KSH */
	setstr(global(version_param), ksh_version);

	/* execute initialization statements */
	for (wp = (char**) initcoms; *wp != NULL; wp++) {
		shcomexec(wp);
		for (; *wp != NULL; wp++)
			;
	}

	if (geteuid() == 0)
		safe_prompt = "# ";
	else
		safe_prompt = "$ ";
	setstr(global("PS1"), safe_prompt);

	/* Set this before parsing arguments */
	Flag(FPRIVILEGED) = getuid() != geteuid() || getgid() != getegid();

	/* this to note if monitor is set on command line (see below) */
	Flag(FMONITOR) = 127;

	argi = parse_args(argv, OF_CMDLINE, (int *) 0);
	if (argi < 0)
		exit(1);

	if (Flag(FCOMMAND)) {
		s = pushs(SSTRING, ATEMP);
		if (!(s->str = argv[argi++]))
			errorf("-c requires an argument");
		if (argv[argi])
			kshname = argv[argi++];
	} else if (argi < argc && !Flag(FSTDIN)) {
		s = pushs(SFILE, ATEMP);
		s->file = argv[argi++];
		s->u.shf = shf_open(s->file, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
		if (s->u.shf == NULL) {
			exstat = 127; /* POSIX */
			errorf("%s: %s", s->file, strerror(errno));
		}
		kshname = s->file;
	} else {
		Flag(FSTDIN) = 1;
		if (isatty(0) && isatty(2))
			Flag(FTALKING) = 1;
		if (Flag(FTALKING)) {
			s = pushs(STTY, ATEMP);
#ifdef EDIT
			x_init();
#endif
		} else {
			s = pushs(SSTDIN, ATEMP);
			s->file = "<stdin>";
			s->u.shf = shf_fdopen(0, SHF_RD | can_seek(0),
				(struct shf *) 0);
		}
	}

	/* This bizarreness is mandated by POSIX */
	{
		struct stat s_stdin;

		if (fstat(0, &s_stdin) >= 0 && S_ISCHR(s_stdin.st_mode))
			reset_nonblock(0);
	}

	/* initialize job control */
	i = Flag(FMONITOR) != 127;
	Flag(FMONITOR) = 0;
	j_init(i);

	l = e->loc;
	l->argv = &argv[argi - 1];
	l->argc = argc - argi;
	l->argv[0] = kshname;
	getopts_reset(1);

	/* Disable during .profile/ENV reading */
	restricted = Flag(FRESTRICTED);
	Flag(FRESTRICTED) = 0;

	/* Do this before profile/$ENV so that if it causes problems in them,
	 * user will know why things broke.
	 */
	if (!current_wd[0] && Flag(FTALKING))
		warningf(FALSE, "Cannot determine current working directory");

	if (Flag(FLOGIN)) {
#ifdef OS2
		char *profile;

		/* Try to find a profile - first see if $INIT has a value,
		 * then try /etc/profile.ksh, then c:/usr/etc/profile.ksh.
		 */
		if (!Flag(FPRIVILEGED)
		    && strcmp(profile = substitute("$INIT/profile.ksh", 0),
			      "/profile.ksh"))
			include(profile, 0, (char **) 0);
		else if (include("/etc/profile.ksh", 0, (char **) 0) < 0)
			include("c:/usr/etc/profile.ksh", 0, (char **) 0);
		if (!Flag(FPRIVILEGED))
			include(substitute("$HOME/profile.ksh", 0), 0,
				(char **) 0);
#else /* OS2 */
		include("/etc/profile", 0, (char **) 0);
		if (!Flag(FPRIVILEGED))
			include(substitute("$HOME/.profile", 0), 0,
				(char **) 0);
#endif /* OS2 */
	}

	if (Flag(FPRIVILEGED))
		include("/etc/suid_profile", 0, (char **) 0);
	else {
		char *env_file;

		/* include $ENV */
		env_file = strval(global("ENV"));
#ifdef DEFAULT_ENV
		/* If env isn't set, include default environment */
		if (env_file == null)
			env_file = DEFAULT_ENV;
#endif /* DEFAULT_ENV */
		env_file = substitute(env_file, DOTILDE);
		if (*env_file != '\0')
			include(env_file, 0, (char **) 0);
#ifdef OS2
		else if (Flag(FTALKING))
			include(substitute("$HOME/kshrc.ksh", 0), 0,
				(char **) 0);
#endif /* OS2 */
	}

	if (is_restricted(argv[0]) || is_restricted(strval(global("SHELL"))))
		restricted = 1;
	if (restricted) {
		static const char *restr_com[] = {
						"typeset", "-r", "PATH",
						    "ENV", "SHELL",
						(char *) 0
					    };
		shcomexec((char **) restr_com);
		/* After typeset command... */
		Flag(FRESTRICTED) = 1;
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
	Source *volatile sold;
	struct shf *shf;
	char **volatile old_argv;
	volatile int old_argc;
	int i;

	shf = shf_open(name, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
	if (shf == NULL)
		return -1;

	if (argv) {
		old_argv = e->loc->argv;
		old_argc = e->loc->argc;
	} else {
		old_argv = (char **) 0;
		old_argc = 0;
	}
	sold = source;
	newenv(E_INCL);
	if ((i = setjmp(e->jbuf))) {
		quitenv();
		source = sold;
		if (s)
			shf_close(s->u.shf);
		if (old_argv) {
			e->loc->argv = old_argv;
			e->loc->argc = old_argc;
		}
		switch (i) {
		  case LRETURN:
		  case LERROR:
			return exstat & 0xff; /* see below */
		  case LINTR:
		  case LEXIT:
		  case LLEAVE:
		  case LSHELL:
			unwind(i);
			/*NOREACHED*/
		  default:
			internal_errorf(1, "include: %d", i);
			/*NOREACHED*/
		}
	}
	if (argv) {
		e->loc->argv = argv;
		e->loc->argc = argc;
	}
	s = pushs(SFILE, ATEMP);
	s->u.shf = shf;
	s->file = strsave(name, ATEMP);
	i = shell(s, FALSE);
	quitenv();
	source = sold;
	shf_close(s->u.shf);
	if (old_argv) {
		e->loc->argv = old_argv;
		e->loc->argc = old_argc;
	}
	return i & 0xff;	/* & 0xff to ensure value not -1 */
}

int
command(comm)
	register char *comm;
{
	register Source *s;

	s = pushs(SSTRING, ATEMP);
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
	volatile int interactive = Flag(FTALKING) && s->type == STTY;
	int i;

	newenv(E_PARSE);
	exstat = 0;
	if (interactive)
		really_exit = 0;
	if ((i = setjmp(e->jbuf))) {
		s->str = null;
		switch (i) {
		  case LINTR: /* we get here if SIGINT not caught or ignored */
		  case LERROR:
		  case LSHELL:
			if (interactive) {
				if (i == LINTR)
					shellf(newline);
				/* Reset any eof that was read as part of a
				 * multiline command.
				 */
				if (Flag(FIGNOREEOF) && s->type == SEOF)
					s->type = STTY;
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
		  default:
			internal_errorf(1, "shell: %d", i);
			/*NOREACHED*/
		}
	}

	while (1) {
		if (trap)
			runtraps(0);

		if (s->next == NULL)
			if (Flag(FVERBOSE))
				s->flags |= SF_ECHO;
			else
				s->flags &= ~SF_ECHO;

		if (interactive)
			j_notify();

		if ((wastty = (s->type == STTY))) {
			set_prompt(PS1, s);
			mcheck();
		}

		t = compile(s);
		if (t != NULL && t->type == TEOF) {
			if (wastty && Flag(FIGNOREEOF) && --attempts > 0) {
				shellf("Use `exit' to leave ksh\n");
				s->type = STTY;
			} else if (wastty && !really_exit
				   && j_stopped_running())
			{
				really_exit = 1;
				s->type = STTY;
			} else {
				/* this for POSIX, which says EXIT traps
				 * shall be taken in the environment
				 * immediately after the last command
				 * executed.
				 */
				if (exit_atend)
					unwind(LEXIT);
				break;
			}
		}

		if (t && (!Flag(FNOEXEC) || s->type == STTY))
			exstat = execute(t, 0);

		if (t != NULL && t->type != TEOF && interactive && really_exit)
			really_exit = 0;

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

	if (ep->oenv == NULL) /* cleanup_parents_env() was called */
		exit(exstat);	/* exit child */
	if (ep->oenv->loc != ep->loc)
		popblock();
	if (ep->savefd != NULL) {
		for (fd = 0; fd < NUFILE; fd++)
			/* if ep->savefd[fd] < 0, means fd was closed */
			if (ep->savefd[fd])
				restfd(fd, ep->savefd[fd]);
		if (ep->savefd[2]) /* Clear any write errors */
			shf_reopen(2, SHF_WR, shl_out);
	}
	reclaim();
	e = e->oenv;
	afree(ep, ATEMP);
}

/* Called after a fork to cleanup stuff left over from parents environment */
void
cleanup_parents_env()
{
	struct env *ep;
	int fd;

	/* Don't clean up temporary files - parent will probably need them.
	 * Also, can't easily reclaim memory since variables, etc. could be
	 * anywyere.
	 */

	/* close all file descriptors hiding in savefd */
	for (ep = e; ep; ep = ep->oenv) {
		if (ep->savefd)
			for (fd = 0; fd < NUFILE; fd++)
				if (ep->savefd[fd] > 0)
					close(ep->savefd[fd]);
	}
	e->oenv = (struct env *) 0;
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

/* Returns true if name refers to a restricted shell */
static int
is_restricted(name)
	char *name;
{
	char *p;

	if ((p = strrchr_dirsep(name)))
		name = p;
	/* accepts rsh, rksh, rpdksh, pdrksh, etc. */
	return (p = strchr(name, 'r')) && strstr(p, "sh");
}

void
aerror(ap, msg)
	Area *ap;
	const char *msg;
{
	internal_errorf(1, "alloc: %s", msg);
	errorf((char *) 0); /* this is never executed - keeps gcc quiet */
	/*NOTREACHED*/
}
