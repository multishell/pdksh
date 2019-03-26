/*
 * Public Domain Bourne/Korn shell
 */

/* $Id: sh.h,v 1.2 1994/05/19 18:32:40 michael Exp michael $ */

#include "config.h"	/* system and option configuration info */

#ifdef HAVE_PROTOTYPES
# define	ARGS(args)	args	/* prototype declaration */
#else
# define	ARGS(args)	()	/* K&R declaration */
#endif

/* Start of common headers */

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#else
/* just a useful subset of what stdlib.h would have */
extern char * getenv  ARGS((const char *));
extern void * malloc  ARGS((size_t));
extern int    free    ARGS((void *));
extern int    exit    ARGS((int));
extern int    rand    ARGS((void));
extern void   srand   ARGS((unsigned int));
extern int    atoi    ARGS((const char *));
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
/* just a useful subset of what unistd.h would have */
extern int access ARGS((const char *, int));
extern int open ARGS((const char *, int, ...));
extern int creat ARGS((const char *, mode_t));
extern int read ARGS((int, char *, unsigned));
extern int write ARGS((int, const char *, unsigned));
extern off_t lseek ARGS((int, off_t, int));
extern int close ARGS((int));
extern int pipe ARGS((int []));
extern int dup2 ARGS((int, int));
extern int unlink ARGS((const char *));
extern int fork ARGS((void));
extern int execve ARGS((const char *, char * const[], char * const[]));
extern int chdir ARGS((const char *));
extern int kill ARGS((pid_t, int));
extern char *getcwd();	/* no ARGS here - differs on different machines */
extern int geteuid ARGS((void));
extern int readlink ARGS((const char *, char *, int));
extern int getegid ARGS((void));
extern int getpid ARGS((void));
extern int getppid ARGS((void));
extern unsigned int sleep ARGS((unsigned int));
extern int isatty ARGS((int));
# ifdef POSIX_PGRP
extern int getpgrp ARGS((void));
extern int setpgid ARGS((pid_t, pid_t));
# endif /* POSIX_PGRP */
# ifdef BSD_PGRP
extern int getpgrp ARGS((pid_t));
extern int setpgrp ARGS((pid_t, pid_t));
# endif /* BSD_PGRP */
# ifdef SYSV_PGRP
extern int getpgrp ARGS((void));
extern int setpgrp ARGS((void));
# endif /* SYSV_PGRP */
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
# define strchr index
# define strrchr rindex
#endif /* HAVE_STRING_H */
#ifndef HAVE_STRSTR
char *strstr ARGS((char *s, char *p));
#endif /* HAVE_STRSTR */
#ifndef HAVE_STRCASECMP
int strcasecmp ARGS((const char *s1, const char *s2));
int strncasecmp ARGS((const char *s1, const char *s2, int n));
#endif /* HAVE_STRCASECMP */

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif
#ifndef HAVE_MEMSET
# define memcpy(d, s, n)	bcopy(s, d, n)
# define memcmp(s1, s2, n)	bcmp(s1, s2, n)
void *memset ARGS((void *d, int c, size_t n));
#endif /* HAVE_MEMSET */
#ifndef HAVE_MEMMOVE
# ifdef HAVE_BCOPY
#  define memmove(d, s, n)	bcopy(s, d, n)
# else
void *memmove ARGS((void *d, const void *s, size_t n));
# endif
#endif /* HAVE_MEMMOVE */

#ifdef HAVE_PROTOTYPES
# include <stdarg.h>
# define SH_VA_START(va, argn) va_start(va, argn)
#else
# include <varargs.h>
# define SH_VA_START(va, argn) va_start(va)
#endif /* HAVE_PROTOTYPES */

#include <errno.h>
extern int errno;

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif /* HAVE_FCNTL_H */
#ifndef O_ACCMODE
# define O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)
#endif /* !O_ACCMODE */

#ifndef F_OK 	/* access() arguments */
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif /* !F_OK */

#ifndef SEEK_SET
# ifdef L_SET
#  define SEEK_SET L_SET
#  define SEEK_CUR L_INCR
#  define SEEK_END L_XTND
# else /* L_SET */
#  define SEEK_SET 0
#  define SEEK_CUR 1
#  define SEEK_END 2
# endif /* L_SET */
#endif /* !SEEK_SET */

/* Some machines (eg, FreeBSD 1.1.5) define CLK_TCK in limits.h
 * (ksh_limval.h assumes limits has been included, if available)
 */
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif /* HAVE_LIMITS_H */

#include <signal.h>
#ifdef	NSIG
# define SIGNALS	NSIG
#else
# ifdef	_MINIX
#  define SIGNALS	(_NSIG+1) /* _NSIG is # of signals used, excluding 0. */
# else
#  ifdef _SIGMAX	/* QNX */
#   define SIGNALS	_SIGMAX
#  else /* _SIGMAX */
#   define SIGNALS	32
#  endif /* _SIGMAX */
# endif	/* _MINIX */
#endif	/* NSIG */
#ifndef SIGCHLD
# define SIGCHLD SIGCLD
#endif
/* struct sigaction.sa_flags is set to KSH_SA_FLAGS.  Used to ensure
 * system calls are interrupted
 */
#ifdef SA_INTERRUPT
# define KSH_SA_FLAGS	SA_INTERRUPT
#else /* SA_INTERRUPT */
# define KSH_SA_FLAGS	0
#endif /* SA_INTERRUPT */

#ifdef USE_FAKE_SIGACT
# include "sigact.h"			/* use sjg's fake sigaction() */
#endif

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif /* HAVE_PATHS_H */
#ifdef _PATH_DEFPATH
# define DEFAULT__PATH _PATH_DEFPATH
#else /* _PATH_DEFPATH */
# define DEFAULT__PATH DEFAULT_PATH
#endif /* _PATH_DEFPATH */

#ifndef offsetof
# define offsetof(type,id) ((size_t)&((type*)NULL)->id)
#endif

#ifndef HAVE_KILLPG
# define killpg(p, s)	kill(-(p), (s))
#endif /* !HAVE_KILLPG */

/* Special cases for execve(2) */
#ifdef OS2
# define ksh_execve(p, av, ev)	_execve(p, av, ev)
#else /* OS2 */
# if defined(OS_ISC) && defined(_POSIX_SOURCE)
/* Kludge for ISC 3.2 (and other versions?) so programs will run correctly.  */
#  define ksh_execve(p, av, ev) do { \
					__setostype(0); \
					execve(p, av, ev); \
					__setostype(1); \
				} while (0)
# else /* OS_ISC && _POSIX */
#  define ksh_execve(p, av, ev)	execve(p, av, ev)
# endif /* OS_ISC && _POSIX */
#endif /* OS2 */

/* this is a hang-over from older versions of the os2 port */
#define ksh_dupbase(fd, base) fcntl(fd, F_DUPFD, base)

/* Find a 32 bit integer type (or die) - SIZEOF_* defined by autoconf
 * (assumes 8 bit byte, but I'm not concerned) */
#if SIZEOF_INT == 4
# define INT32	int
#else /* SIZEOF_INT */
# if SIZEOF_LONG == 4
#  define INT32	long
# else /* SIZEOF_LONG */
   #error cannot find 32 bit type...
# endif /* SIZEOF_LONG */
#endif /* SIZEOF_INT */

/* end of common headers */

/* Stop gcc and lint from complaining about possibly uninitialized variables */
#if defined(__GNUC__) || defined(lint)
# define UNINITIALIZED(var)	var = 0
#else
# define UNINITIALIZED(var)	var
#endif /* GNUC || lint */

/* some useful #defines */
#ifdef EXTERN
# define _I_(i) = i
#else
# define _I_(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#ifndef EXECSHELL
/* shell to exec scripts (see also $SHELL initialization in main.c) */
# ifdef OS2
#  define EXECSHELL	"c:\\os2\\cmd.exe"
#  define EXECSHELL_STR	"OS2_SHELL"
# else /* OS2 */
#  define EXECSHELL	"/bin/sh"
#  define EXECSHELL_STR	"EXECSHELL"
# endif /* OS2 */
#endif

/* ISABSPATH() means path is fully and completely specified,
 * ISROOTEDPATH() means a .. as the first component is a no-op,
 * ISRELPATH() means $PWD can be tacked on to get an absolute path.
 *
 * OS	Path		ISABSPATH	ISROOTEDPATH	ISRELPATH
 * unix	/foo		yes		yes		no
 * unix	foo		no		no		yes
 * unix	../foo		no		no		yes
 * os2	a:/foo		yes		yes		no
 * os2	a:foo		no		no		no
 * os2	/foo		no		yes		no
 * os2	foo		no		no		yes
 * os2	../foo		no		no		yes
 */
#ifdef OS2
# define PATHSEP        ';'
# define DIRSEP         '\\'
# define DIRSEPSTR      "\\"
# define ISDIRSEP(c)    ((c) == '\\' || (c) == '/')
# define ISABSPATH(s)	(((s)[0] && (s)[1] == ':' && ISDIRSEP((s)[2])))
# define ISROOTEDPATH(s) (ISDIRSEP((s)[0]) || ISABSPATH(s))
# define ISRELPATH(s)	(!(s)[0] || ((s)[1] != ':' && !ISDIRSEP((s)[0])))
# define FILECHCONV(c)	(isupper(c) ? tolower(c) : c)
# define FILECMP(s1, s2) stricmp(s1, s2)
# define FILENCMP(s1, s2, n) strnicmp(s1, s2, n)
extern char *strchr_dirsep(char *path);
extern char *strrchr_dirsep(char *path);
# define chdir          _chdir2
# define getcwd         _getcwd2
#else
# define PATHSEP        ':'
# define DIRSEP         '/'
# define DIRSEPSTR      "/"
# define ISDIRSEP(c)    ((c) == '/')
# define ISABSPATH(s)	ISDIRSEP((s)[0])
# define ISROOTEDPATH(s) ISABSPATH(s)
# define ISRELPATH(s)	(!ISABSPATH(s))
# define FILECHCONV(c)	c
# define FILECMP(s1, s2) strcmp(s1, s2)
# define FILENCMP(s1, s2, n) strncmp(s1, s2, n)
# define strchr_dirsep(p)   strchr(p, DIRSEP)
# define strrchr_dirsep(p)  strrchr(p, DIRSEP)
#endif

typedef int bool_t;
#define	FALSE	0
#define	TRUE	1

#define	NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define	sizeofN(type, n) (sizeof(type) * (n))
#define	BIT(i)	(1<<(i))	/* define bit in flag */

#define	NUFILE	10		/* Number of user-accessible files */
#define	FDBASE	10		/* First file usable by Shell */

/* you're not going to run setuid shell scripts, are you? */
#define	eaccess(path, mode)	access(path, mode)

/* make MAGIC a char that might be printed to make bugs more obvious */
#define	MAGIC	(char)(0x80+'!')/* prefix for *?[!{,} during expand */
#define	NOT	'!'		/* might use ^ (ie, [!...] vs [^..]) */

#define	LINE	1024		/* input line size */
#define	PATH	1024		/* pathname size (todo: PATH_MAX/pathconf()) */
#define ARRAYMAX 1023		/* max array index */

EXTERN	char	*kshname;	/* $0 */
EXTERN	pid_t	kshpid;		/* $$, shell pid */
EXTERN	pid_t	procpid;	/* pid of executing process */
EXTERN	int	exstat;		/* exit status */
EXTERN	int	subst_exstat;	/* exit status of last $(..)/`..` */
EXTERN	char	*safe_prompt;	/* safe prompt if PS1 substitution fails */


/*
 * Area-based allocation built on malloc/free
 */

typedef struct Area {
	struct Block *freelist;	/* free list */
} Area;

EXTERN	Area	aperm;		/* permanent object space */
#define	APERM	&aperm
#define	ATEMP	&e->area

#ifdef MEM_DEBUG
# include "chmem.h" /* a debugging front end for malloc et. al. */
#endif /* MEM_DEBUG */


/*
 * parsing & execution environment
 */
EXTERN	struct env {
	short	type;			/* enviroment type - see below */
	short	flags;			/* EF_* */
	Area	area;			/* temporary allocation area */
	struct	block *loc;		/* local variables and functions */
	short  *savefd;			/* original redirected fd's */
	struct	env *oenv;		/* link to previous enviroment */
	jmp_buf	jbuf;			/* long jump back to env creator */
	struct temp *temps;		/* temp files */
} *e;

/* struct env.type values */
#define	E_NONE	0		/* dummy enviroment */
#define	E_PARSE	1		/* parsing command # */
#define	E_FUNC	2		/* executing function # */
#define	E_INCL	3		/* including a file via . # */
#define	E_EXEC	4		/* executing command tree */
#define	E_LOOP	5		/* executing for/while # */
#define	E_ERRH	6		/* general error handler # */
/* # indicates env has valid jbuf (see unwind()) */

/* struct env.flag values */
#define EF_FUNC_PARSE	BIT(0)	/* function being parsed */
#define EF_BRKCONT_PASS	BIT(1)	/* set if E_LOOP must pass break/continue on */

/* Do breaks/continues stop at env type e? */
#define STOP_BRKCONT(t)	((t) == E_NONE || (t) == E_PARSE \
			 || (t) == E_FUNC || (t) == E_INCL)
/* Do returns stop at env type e? */
#define STOP_RETURN(t)	((t) == E_FUNC || (t) == E_INCL)

/* values for longjmp(e->jbuf) */
#define LRETURN	1		/* return statement */
#define	LEXIT	2		/* exit statement */
#define LERROR	3		/* errorf() called */
#define LLEAVE	4		/* untrappable exit/error */
#define LINTR	5		/* ^C noticed */
#define	LBREAK	6		/* break statement */
#define	LCONTIN	7		/* continue statement */
#define LSHELL	8		/* return to interactive shell() */
#define LAEXPR	9		/* error in arithmetic expression */


/* option processing */
#define OF_CMDLINE	0x01	/* command line */
#define OF_SET		0x02	/* set builtin */
#define OF_SPECIAL	0x04	/* a special variable changing */
#define OF_ANY		(OF_CMDLINE | OF_SET | OF_SPECIAL)

struct option {
    char	*name;	/* long name of option */
    char	c;	/* character flag (if any) */
    short	flags;	/* OF_* */
};
extern struct option options[];

/*
 * flags (the order of these enums MUST match the order in misc.c(options[]))
 */
enum sh_flag {
	FEXPORT = 0,	/* -a: export all */
#ifdef BRACEEXPAND
	FBRACEEXPAND,	/* enable {} globing */
#endif
	FBGNICE,	/* bgnice */
	FCOMMAND,	/* -c: (invocation) execute specified command */
#ifdef EMACS
	FEMACS,		/* emacs command editing */
#endif
	FERREXIT,	/* -e: quit on error */
#ifdef EMACS
	FGMACS,		/* gmacs command editing */
#endif
	FIGNOREEOF,	/* eof does not exit */
	FTALKING,	/* -i: interactive */
	FKEYWORD,	/* -k: name=value anywere */
	FLOGIN,		/* -l: a login shell */
	FMARKDIRS,	/* mark dirs with / in file name completion */
	FMONITOR,	/* -m: job control monitoring */
	FNOCLOBBER,	/* -C: don't overwrite existing files */
	FNOEXEC,	/* -n: don't execute any commands */
	FNOGLOB,	/* -f: don't do file globbing */
	FNOHUP,		/* -H: don't kill running jobs when login shell exits */
	FNOLOG,		/* don't save functions in history (ignored) */
#ifdef	JOBS
	FNOTIFY,	/* -b: asynchronous job completion notification */
#endif
	FNOUNSET,	/* -u: using an unset var is an error */
	FPHYSICAL,	/* -o physical: don't do logical cd's/pwd's */
	FPOSIX,		/* -o posix: be posixly correct */
	FPRIVILEGED,	/* -p: use suid_profile */
	FRESTRICTED,	/* -r: restricted shell */
	FSTDIN,		/* -s: (invocation) parse stdin */
	FTRACKALL,	/* -h: create tracked aliases for all commands */
	FVERBOSE,	/* -v: echo input */
#ifdef VI
	FVI,		/* vi command editing */
	FVIRAW,		/* always read in raw mode (ignored) */
	FVISHOW8,	/* display chars with 8th bit set as is (versus M-) */
	FVITABCOMPLETE,	/* enable tab as file name completion char */
#endif
	FXTRACE,	/* -x: execution trace */
	FNFLAGS /* (place holder: how many flags are there) */
};

#define Flag(f)	(shell_flags[(int) (f)])

EXTERN	char shell_flags [FNFLAGS];

EXTERN	char	null [] _I_("");	/* null value for variable */
EXTERN	char	space [] _I_(" ");
EXTERN	char	newline [] _I_("\n");
EXTERN	char	slash [] _I_("/");

typedef	RETSIGTYPE (*handler_t)();	/* signal handler */

/* temp/here files. the file is removed when the struct is freed */
struct temp {
	struct temp	*next;
	int		pid;		/* pid of process parsed here-doc */
	char		*name;
};

/* here documents in functions are treated specially (the get removed when
 * shell exis) */
EXTERN struct temp	*func_heredocs;

/*
 * stdio and our IO routines
 */

#define shl_spare	(&shf_iob[0])	/* for c_read()/c_print() */
#define shl_stdout	(&shf_iob[1])
#define shl_out		(&shf_iob[2])
EXTERN int shl_stdout_ok;

/*
 * trap handlers
 */
typedef struct trap {
	int	signal;		/* signal number */
	char   *name;		/* short name */
	const char *mess;	/* descriptive name */
	char   *trap;		/* trap command */
	int	volatile set;	/* trap pending */
	int	flags;		/* TF_* */
	RETSIGTYPE (*cursig)();	/* current handler (valid if TF_ORIG_* set) */
} Trap;

/* values for Trap.flags */
#define TF_SHELL_USES	BIT(0)	/* shell uses signal, user can't change */
#define TF_USER_SET	BIT(1)	/* user has (tried to) set trap */
#define TF_ORIG_IGN	BIT(2)	/* original action was SIG_IGN */
#define TF_ORIG_DFL	BIT(3)	/* original action was SIG_DFL */
#define TF_EXEC_IGN	BIT(4)	/* restore SIG_IGN just before exec */
#define TF_EXEC_DFL	BIT(5)	/* restore SIG_DFL just before exec */
#define TF_DFL_INTR	BIT(6)	/* when received, default action is LINTR */
#define TF_TTY_INTR	BIT(7)	/* tty generated signal (see j_waitj) */
#define TF_CHANGED	BIT(8)	/* used by runtrap() to detect trap changes */
#define TF_FATAL	BIT(9)	/* causes termination if not trapped */

/* values for setsig()/setexecsig() flags argument */
#define SS_RESTORE_MASK	0x3	/* how to restore a signal before an exec() */
#define SS_RESTORE_CURR	0	/* leave current handler in place */
#define SS_RESTORE_ORIG	1	/* restore original handler */
#define SS_RESTORE_DFL	2	/* restore SIG_DFL */
#define SS_RESTORE_IGN	3	/* restore SIG_IGN */
#define SS_FORCE	BIT(3)	/* set signal even if original signal ignored */
#define SS_USER		BIT(4)	/* user is doing the set (ie, trap command) */

#define SIGEXIT_	0	/* for trap EXIT */
#define SIGERR_		SIGNALS	/* for trap ERR */

EXTERN	int volatile trap;	/* traps pending? */
EXTERN	int volatile intrsig;	/* pending trap interrupts executing command */
EXTERN	int volatile fatal_trap;/* received a fatal signal */
extern	Trap	sigtraps[SIGNALS+1];


/*
 * TMOUT support
 */
/* values for ksh_tmout_state */
enum tmout_enum {
		TMOUT_EXECUTING	= 0,	/* executing commands */
		TMOUT_READING,		/* waiting for input */
		TMOUT_LEAVING		/* have timed out */
	};
EXTERN unsigned int ksh_tmout _I_(0);
EXTERN enum tmout_enum ksh_tmout_state _I_(TMOUT_EXECUTING);


/* For "You have stopped jobs" message */
EXTERN int really_exit;


/*
 * fast character classes
 */
#define	C_ALPHA	0x01		/* a-z_A-Z */
#define	C_DIGIT	0x02		/* 0-9 */
#define	C_LEX1	0x04		/* \0 \t\n|&;<>() */
#define	C_VAR1	0x08		/* *@#!$-? */
#define	C_IFSWS	0x10		/* \t \n (IFS white space) */
#define	C_SUBOP1 0x20		/* "=-+?" */
#define	C_SUBOP2 0x40		/* "#%" */
#define	C_IFS	0x80		/* $IFS */

extern	char ctypes [];

#define	ctype(c, t)	!!(ctypes[(unsigned char)(c)]&(t))
#define	letter(c)	ctype(c, C_ALPHA)
#define	digit(c)	ctype(c, C_DIGIT)
#define	letnum(c)	ctype(c, C_ALPHA|C_DIGIT)

EXTERN int ifs0 _I_(' ');	/* for "$*" */


/* Argument parsing for built-in commands and getopts command */

/* Values for Getopt.flags */
#define GF_ERROR	BIT(0)	/* call errorf() if there is an error */
#define GF_PLUSOPT	BIT(1)	/* allow +c as an option */

/* Values for Getopt.info */
#define GI_NONAME	BIT(0)	/* don't print argv[0] in errors */
#define GI_MINUS	BIT(1)	/* an option started with -... */
#define GI_PLUS		BIT(2)	/* an option started with +... */
#define GI_MINUSMINUS	BIT(3)	/* arguments were ended with -- */

typedef struct {
	int	optind;
	char	*optarg;
	int	flags;		/* see GF_* */
	int	info;		/* see GI_* */
	int	p;		/* 0 or index into argv[optind - 1] */
	char	buf[2];		/* for bad option OPTARG value */
} Getopt;

EXTERN Getopt builtin_opt;	/* for shell builtin commands */


/* This for co-processes */
struct coproc {
	int	read;		/* pipe from co-process's stdout */
	int	readw;		/* other side of read (saved temporarily) */
	int	write;		/* pipe to co-process's stdin */
	void	*job;		/* 0 if no co-process, or co-process died */
};
EXTERN struct coproc coproc;

extern char ksh_version[];

/* name of called builtin function (used by error functions) */
EXTERN char	*builtin_argv0;
EXTERN int	builtin_flag;	/* flags of called builtin (SPEC_BI, etc.) */

/* current working directory, and size of memory allocated for same */
EXTERN char	*current_wd;
EXTERN int	current_wd_size;

#ifdef EDIT
/* Minimium required space to work with on a line - if the prompt leaves less
 * space than this on a line, the prompt is truncated.
 */
# define MIN_EDIT_SPACE	7
/* Minimium allowed value for x_cols: 2 for prompt, 3 for " < " at end of line
 */
# define MIN_COLS	(2 + MIN_EDIT_SPACE + 3)
EXTERN	int	x_cols _I_(80);	/* tty columns */
#else
# define x_cols 80		/* for pr_menu(exec.c) */
#endif


/* These to avoid bracket matching problems */
#define OPAREN	'('
#define CPAREN	')'
#define OBRACK	'['
#define CBRACK	']'
#define OBRACE	'{'
#define CBRACE	'}'


#include "shf.h"
#include "table.h"
#include "tree.h"
#include "expand.h"
#include "lex.h"
#include "proto.h"

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef _I_
