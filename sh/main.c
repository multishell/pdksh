/*
 * startup, main loop, enviroments and error handling
 */

#ifndef lint
static char *RCSid = "$Id: main.c,v 1.4 1992/04/29 06:25:47 sjg Exp $";
#endif

#define	Extern				/* define Externs in sh.h */

#include "stdh.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <time.h>
#include "sh.h"

#if !defined(HAVE_REMOVE) && !defined(remove)
#define remove(x)	unlink(x)
#endif

/*
 * global data
 */

Area	aperm;

static	void	reclaim ARGS((void));

/*
 * shell initialization
 */

static	char	initifs [] = "IFS= \t\n"; /* must be R/W */

static	const	char   initsubs [] = 
#ifdef sun				/* sun's don't have a real /bin */
  "${SHELL:=/bin/sh} ${PATH:=/usr/bin:/usr/ucb:.} ${HOME:=/} ${PS1:=$ } ${PS2:=> } ${MAILCHECK:=600}";
#else
  "${SHELL:=/bin/sh} ${PATH:=/bin:/usr/bin:.} ${HOME:=/} ${PS1:=$ } ${PS2:=> } ${MAILCHECK:=600}";
#endif

static	const	char *initcoms [] = {
	"cd", ".", NULL,		/* set up $PWD */
	"typeset", "-x", "SHELL", "PATH", "HOME", NULL,
	"typeset", "-r", "PWD", "OLDPWD", NULL,
	"typeset", "-i", "SECONDS=0", "OPTIND=1", NULL,
	"alias",
	  "integer=typeset -i", "pwd=print -r \"$PWD\"",
	  "history=fc -l", "r=fc -e -", "nohup=nohup ",
	  "login=exec login", "newgrp=exec newgrp",
	  "type=whence -v", "functions=typeset -f",
	  "echo=print", "true=:", "false=let", "[=\\[", NULL,
	NULL
};

#ifdef USE_TRACE
/*
 * use SIGUSR1 to bump up Trace_level
 * use SIGUSR2 to clear Trace_level
 */
void
set_TraceLev(sig)
  int sig;
{
  switch(sig)
  {
  case SIGUSR1:
    Trace_level++;
    break;
  case SIGUSR2:
    Trace_level = 0;
    break;
  }
#if defined(_SYSV) && !defined(USE_SIGACT)
  if (sig > 0)
    (void) signal(sig, set_TraceLev);
#endif
  return;
}
#endif

main(argc, argv, envp)
	int argc;
	register char **argv;
	char **envp;
{
	register int i;
	register char *arg;
	int cflag = 0, qflag = 0, fflag = 0;
	int argi;
	char *name;
	register Source *s;
	register struct block *l = &globals;
	register char **wp0, **wp;
	extern char ksh_version [];
	extern time_t time();

#ifdef USE_SIGACT
	sigemptyset(&Sigact.sa_mask);
	sigemptyset(&Sigact_dfl.sa_mask);
	sigemptyset(&Sigact_ign.sa_mask);
	sigemptyset(&Sigact_trap.sa_mask);
	Sigact.sa_flags = 0;
	Sigact_dfl.sa_flags = 0;
	Sigact_ign.sa_flags = 0;
	Sigact_trap.sa_flags = 0;
	Sigact_dfl.sa_handler = SIG_DFL;
	Sigact_ign.sa_handler = SIG_IGN;
	Sigact_trap.sa_handler = trapsig;
#endif
	ainit(&aperm);		/* initialize permanent Area */

#ifndef F_SETFD
  init_clexec();
#endif
	/* set up base enviroment */
	e.type = E_NONE;
	ainit(&e.area);
	e.loc = l;
	e.savefd = NULL;
	e.oenv = NULL;

	initctypes();

	/* open file streams for fd's 0,1,2 */
	fopenshf(0);	fopenshf(1);	fopenshf(2);

	/* set up variable and command dictionaries */
	newblock();		/* set up global l->vars and l->funs */
	tinit(&commands, APERM);
	tinit(&builtins, APERM);
	tinit(&lexicals, APERM);
	tinit(&homedirs, APERM);

	/* import enviroment */
	if (envp != NULL)
		for (wp = envp; *wp != NULL; wp++)
			import(*wp);

	kshpid = getpid();
	typeset(initifs, 0, 0);	/* for security */
	typeset(ksh_version, 0, 0); /* RDONLY */

#ifdef USE_TRACE
#ifdef USE_SIGACT
	Sigact.sa_handler = set_TraceLev;
	sigaction(SIGUSR1, &Sigact, NULL);
	sigaction(SIGUSR2, &Sigact, NULL);
#else
	(void) signal(SIGUSR1, set_TraceLev);
	(void) signal(SIGUSR2, set_TraceLev);
#endif
	_TRACE(0, ("Traces enabled.")); /* allow _TRACE to setup */
#endif

	/* define shell keywords */
	keywords();

	/* define built-in commands */
	for (i = 0; shbuiltins[i].name != NULL; i++)
		builtin(shbuiltins[i].name, shbuiltins[i].func);
	for (i = 0; kshbuiltins[i].name != NULL; i++)
		builtin(kshbuiltins[i].name, kshbuiltins[i].func);

	/* assign default shell variable values */
	substitute(initsubs, 0);
	setint(typeset("PPID", INTEGER, 0), (long) getppid());
	typeset("PPID", RDONLY, 0);
	setint(typeset("RANDOM", INTEGER, 0), (long) time((time_t *)0));
	/* execute initialization statements */
	for (wp0 = (char**) initcoms; *wp0 != NULL; wp0 = wp+1) {
		/* copy because the alias initializers are readonly */
		for (wp = wp0; *wp != NULL; wp++)
			*wp = strsave(*wp, ATEMP);
		shcomexec(wp0);
	}
	afreeall(ATEMP);

	if (geteuid() == 0)
		setstr(global("PS1"), "# ");

	s = pushs(SFILE);
	s->u.file = stdin;
	cflag = 0;
	name = *argv;

	/* what a bloody mess */
	if ((argi = 1) < argc) {
		if (argv[argi][0] == '-' && argv[argi][1] != '\0') {
			for (arg = argv[argi++]+1; *arg; arg++) {
				switch (*arg) {
				  case 'c':
					cflag = 1;
					if (argi < argc) {
						s->type = SSTRING;
						s->str = argv[argi++];
					}
					break;
	
				  case 'q':
					qflag = 1;
					break;

				  default:
					if (*arg>='a' && *arg<='z')
						flag[FLAG(*arg)]++;
				}
			}
		}
		if (s->type == SFILE && argi < argc && !flag[FSTDIN]) {
			s->file = name = argv[argi++];
			if ((s->u.file = fopen(name, "r")) == NULL)
				errorf("%s: cannot open\n", name);
			fflag = 1;
			fileno(s->u.file) = savefd(fileno(s->u.file));
			setvbuf(s->u.file, (char *)NULL, _IOFBF, BUFSIZ);
		}
	}

	if (s->type == SFILE) {
		if (fileno(s->u.file) == 0)
			flag[FSTDIN] = 1;
		if (isatty(0) && isatty(1) && !cflag && !fflag)
			flag[FTALKING] = 1;
		if (flag[FTALKING] && flag[FSTDIN])
			s->type = STTY;
	}
	if (s->type == STTY) {
		ttyfd = fcntl(0, F_DUPFD, FDBASE);
#ifdef F_SETFD
		(void) fcntl(ttyfd, F_SETFD, 1);
#else
		(void) fd_clexec(ttyfd);
#endif
#ifdef EMACS
		x_init_emacs();
#endif
	}

	/* initialize job control */
	j_init();

	if (!qflag)
		ignoresig(SIGQUIT);

	l->argv = &argv[argi];
	l->argc = argc - argi;
	l->argv[0] = name;
	resetopts();

	if (name[0] == '-') {
		flag[FTALKING] = 1;
		(void) include("/etc/profile");
		(void) include(".profile");
	}

	/* include $ENV */
	arg = substitute(strval(global("ENV")), DOTILDE);
	if (*arg != '\0')
		(void) include(arg);

	if (flag[FTALKING])
	{
#ifdef USE_SIGACT
	  sigaction(SIGTERM, &Sigact_trap, NULL);
#else
	  signal(SIGTERM, trapsig);
#endif
	  ignoresig(SIGINT);
#if defined(EMACS) || defined(VI)
	  init_editmode();
#endif
	} else
	  flag[FHASHALL] = 1;

#ifdef JOBS			/* todo: could go before includes? */
	if (s->type == STTY) {
		flag[FMONITOR] = 1;
		j_change();
	}
#endif
	if (flag[FTALKING])
	{
	  hist_init(s);
	}
	argc = shell(s);
	leave(argc);
	return 0;
}

int
include(name)
	register char *name;
{
	register FILE *f;
	register Source *s;

	if (strcmp(name, "-") != 0) {
		f = fopen(name, "r");
		if (f == NULL)
			return 0;
		/* todo: the savefd doesn't get popped */
		fileno(f) = savefd(fileno(f)); /* questionable */
		setvbuf(f, (char *)NULL, _IOFBF, BUFSIZ);
	} else
		f = stdin;
	s = pushs(SFILE);
	s->u.file = f;
	s->file = name;
	/*return*/ shell(s);
	if (f != stdin)
		fclose(f);
	return 1;
}

int
command(comm)
	register char *comm;
{
	register Source *s;

	s = pushs(SSTRING);
	s->str = comm;
	return shell(s);
}

/*
 * run the commands from the input source, returning status.
 */
int
shell(s)
	Source *s;		/* input source */
{
	struct op *t;
	volatile int attempts = 13;
	volatile int wastty;
	volatile int reading = 0;
	extern void mcheck();

	newenv(E_PARSE);
	e.interactive = 1;
	exstat = 0;
	if (setjmp(e.jbuf)) {
		/*shellf("<unwind>");*/
		if (trap)	/* pending SIGINT */
			shellf("\n");
		if (reading && s->type == STTY && s->line)
			s->line--;
		sigtraps[SIGINT].set = 0;
	}

	while (1) {
		if (trap)
			runtraps();
		if (flag[FTALKING])
		{
#ifdef USE_SIGACT
		  sigaction(SIGINT, &Sigact_trap, NULL);
#else
		  signal(SIGINT, trapsig);
#endif
		}

		if (s->next == NULL)
			s->echo = flag[FVERBOSE];

		j_notify();

		if ((wastty = (s->type == STTY)) || s->type == SHIST) {
			prompt = substitute(strval(global("PS1")), 0);
			mcheck();
		}

		reading = 1;
		t = compile(s);
		reading = 0;
		j_reap();
		if (t != NULL && t->type == TEOF)
			if (wastty && flag[FIGNEOF] && --attempts > 0) {
				shellf("Use `exit'\n");
				s->type = STTY;
				continue;
			}
			else
				break;
		flushshf(2);	/* flush -v output */

		if (!flag[FNOEXEC] || s->type == STTY)
			execute(t, 0);

		reclaim();
	}
	/* Error: */
	quitenv();
	return exstat;
}

void
leave(rv)
	int rv;
{
	if (e.type == E_TCOM && e.oenv != NULL)	/* exec'd command */
		unwind();
	runtrap(&sigtraps[0]);
	if (flag[FTALKING])
	{
	  hist_finish();
	}
	j_exit();
	exit(rv);
	/* NOTREACHED */
}

error()
{
	if (flag[FERREXIT] || !flag[FTALKING])
		leave(1);
	unwind();
}

/* return to closest error handler or shell(), exit if none found */
unwind()
{
	while (1)
		switch (e.type) {
		  case E_NONE:
			leave(1);
			/* NOTREACHED */
		  case E_PARSE:
			longjmp(e.jbuf, 1);
			/* NOTREACHED */
		  case E_ERRH:
			longjmp(e.jbuf, 1);
			/* NOTREACHED */
		  default:
			quitenv();
			break;
		}
}

newenv(type)
{
	register struct env *ep;

	ep = (struct env *) alloc(sizeof(*ep), ATEMP);
	*ep = e;
	ainit(&e.area);
	e.type = type;
	e.oenv = ep;
	e.savefd = NULL;
	e.temps = NULL;
}

quitenv()
{
	register struct env *ep;
	register int fd;

	if ((ep = e.oenv) == NULL)
		exit(exstat);	/* exit child */
	if (e.loc != ep->loc)
		popblock();
	if (e.savefd != NULL)
		for (fd = 0; fd < NUFILE; fd++)
			restfd(fd, e.savefd[fd]);
	reclaim();
	e = *ep;
	afree(ep, ATEMP);
}

/* remove temp files and free ATEMP Area */
static void
reclaim()
{
	register struct temp *tp;

	for (tp = e.temps; tp != NULL; tp = tp->next)
		remove(tp->name);
	e.temps = NULL;
	afreeall(&e.area);
}

void
aerror(ap, msg)
	Area *ap;
	const char *msg;
{
	errorf("alloc internal error: %s\n", msg);
}

