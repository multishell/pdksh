/*
 * Public Domain Bourne/Korn shell
 */

/* $Id: sh.h,v 1.3 1992/04/25 08:29:52 sjg Exp $ */

#include "config.h"

/* allow for non-Unix linkers. main.c has a "#define Extern " */
#ifndef Extern
# define Extern	extern
#endif

#ifndef SHELL
#define	SHELL	"/bin/sh"	/* shell to exec scripts */
#endif

/* some people object to this use of __STDC__ */
#ifdef __STDC__
#define	ARGS(args)	args	/* prototype declaration */
#else
#define	ARGS(args)	()	/* K&R declaration */
#ifdef VOID
#define	void	VOID
#endif
#define	const	
#define	volatile 
#endif

#ifdef _ULTRIX			/* Ultrix 2.x gets dup2 wrong */
int dup2 ARGS ((int, int));
/* assumes we don't want dup2 return value */
#define	dup2(ofd, nfd) \
		(void) ((dup2)(ofd, nfd), fcntl(nfd, F_SETFD, 0))
#endif

#if defined(EMACS) || defined(VI)
#define	EDIT
#endif

typedef int bool_t;
#define	FALSE	0
#define	TRUE	1

#define	sizeofN(type, n) (sizeof(type) * (n))
#define	BIT(i)	(1<<(i))	/* define bit in flag */

#define	NUFILE	10		/* Number of user-accessible files */
#define	FDBASE	10		/* First file usable by Shell */

/* you're not going to run setuid shell scripts, are you? */
#define	eaccess(path, mode)	access(path, mode)

#define	MAGIC	(char)0x80	/* prefix for ~*?[ during expand */
#define	NOT	'!'	/* might use ^ */

#define	LINE	256		/* input line size */
#define	PATH	256		/* pathname size */

Extern	int	kshpid;		/* $$, shell pid */
Extern	int	exstat;		/* exit status */
Extern	int	async;		/* $!, last &'d pid */
Extern	volatile int sigchld_caught;	/* count of dead children */


/*
 * Area-based allocation built on malloc/free
 */

typedef struct Area {
	struct Block *free;	/* free list */
} Area;

extern	Area	aperm;		/* permanent object space */
#define	APERM	&aperm
#define	ATEMP	&e.area

/*
 * parsing & execution environment
 */
Extern	struct	env {
	int	type;			/* enviroment type - see below */
	Area	area;			/* temporary allocation area */
	struct	block *loc;		/* local variables and functions */
	short  *savefd;			/* original redirected fd's */
	struct	env *oenv;		/* link to previous enviroment */
	jmp_buf	jbuf;			/* long jump back to env creator */
	int	interactive;		/* fd's 0,1,2 are tty */
	struct temp *temps;		/* temp files */
} e;

#define	E_NONE	0		/* dummy enviroment */
#define	E_PARSE	1		/* parsing command # */
#define	E_EXEC	2		/* executing command tree */
#define	E_LOOP	3		/* executing for/while # */
#define	E_TCOM	5		/* executing simple command */
#define	E_FUNC	6		/* executing function */
#define	E_ERRH	7		/* general error handler # */
/* # indicates env has valid jbuf */

/*
 * flags
 */
#define	FEXPORT	FLAG('a')	/* -a: allexport */
#define	FERREXIT FLAG('e')	/* -e: errexit (quit on error) */
#define	FBGNICE	29		/* bgnice */
#define	FEMACS 30		/* emacs command editing */
#define	FIGNEOF	27		/* ignoreeof (eof does not exit) */
#define	FHASHALL FLAG('h')	/* -h: trackall, hashall */
#define	FTALKING FLAG('i')	/* -i: interactive (talking type wireless) */
#define	FKEYWORD FLAG('k')	/* -k: keyword (name=value anywhere) */
#define	FMARKDIRS 28		/* markdirs */
#define	FMONITOR FLAG('m')	/* -m: monitor */
#define	FNOEXEC	FLAG('n')	/* -n: noexec */
#define	FNOGLOB	FLAG('f')	/* -f: noglob */
#define	FPRIVILEGED FLAG('p')	/* -p: privileged */
#define	FSTDIN	FLAG('s')	/* -s (invocation): parse stdin */
#define	FNOUNSET FLAG('u')	/* -u: nounset (unset vars is error) */
#define	FVERBOSE FLAG('v')	/* -v: verbose (echo input) */
#define	FVI 31			/* vi command editing */
#define	FXTRACE	FLAG('x')	/* -x: (execute) xtrace */

#define	FLAG(c)	(1 + c - 'a')	/* map char to flags index */
#define	FLAGS	32
Extern	char flag [FLAGS];
int	option ARGS((const char *name));
char   *getoptions ARGS((void));
void	printoptions ARGS((void));

extern	char	null [];	/* null value for variable */

/*
 * other functions
 */
char * substitute ARGS((char const *, int));
char   *search();
struct tbl *findcom();
char   *strsave ARGS((char *, Area *));
char   *ulton ARGS((unsigned long n, int base));
int	xstrcmp();
void	qsortp ARGS((void **base, size_t n, int (*compare)(void *, void *)));
long	evaluate ARGS((const char *expr));
void	resetopts();
void	histsave();
void	histlist();

void	j_init ARGS((void));
void	j_exit ARGS((void));
void	j_notify ARGS((void));
void	j_kill ARGS((int job, int sig));
#ifdef JOBS
void	j_change ARGS((void));
int	j_resume ARGS((int job, int bg));
#endif

/*
 * error handling
 */
void	leave();	/* abort shell (or fail in subshell) */

/*
 * library functions
 */
typedef	void (*handler_t)();	/* signal handler */

/* temp/here files. the file is removed when the struct is freed */
struct	temp {
	struct	temp * next;
	char   *name;
};
struct temp *maketemp ARGS((Area *ap));

/*
 * stdio and our IO routines
 */

#ifdef	BUFSIZ			/* <stdio.h> included? */
extern	FILE *	shf [NUFILE];	/* map shell fd to FILE */
#endif
void	fopenshf();
void	flushshf();

#undef	stdin
#undef	stdout

#define	stdin	shf[0]		/* standard input */
#define	stdout	shf[1]		/* standard output */
#define	shlout	shf[2]		/* shell output */

int	shellf ARGS((const char *fmt, ...)); /* fprintf(shlout, ); */
int	errorf ARGS((const char *fmt, ...)); /* fprintf(shlout, ); error(); */

/*
 * IO control
 */
extern	int ttyfd;		/* tty fd (original fd 0) */

int	savefd ARGS((int fd));	/* save user fd */
void	restfd ARGS((int fd, int ofd));
void	openpipe ARGS((int [2]));
void	closepipe ARGS((int [2]));

/*
 * trap handlers
 */
typedef struct trap {
	int	signal;		/* signal number */
	char   *name;		/* short name */
	char   *mess;		/* descriptive name */
	char   *trap;		/* trap command */
	int	volatile set;	/* trap pending */
	int	ourtrap;	/* not ignored (?) */
	int	sig_dfl;	/* originally SIG_DFL */
} Trap;

#ifndef	SIGKILL
#include <signal.h>
#endif	/* SIGKILL */
#ifdef	NSIG
#define	SIGNALS	NSIG
#else
#ifdef	_MINIX
#define	SIGNALS	(_NSIG+1)	/* _NSIG is # of signals used, excluding 0. */
#else
#define	SIGNALS	32
#endif	/* _MINIX */
#endif	/* NSIG */

#ifdef USE_SIGACT			/* always use it? */
#ifndef  SA_NOCLDSTOP
# include "sigact.h"			/* use sjg's fake sigaction() */
#endif
Extern struct sigaction Sigact, Sigact_dfl, Sigact_ign, Sigact_trap;
#endif

Extern	int volatile trap;	/* traps pending? */
extern	Trap	sigtraps[SIGNALS];
Trap    *gettrap ARGS((char *)); /* search for struct trap by number or name */
void	trapsig ARGS((int sig)); /* trap signal handler */

/*
 * fast character classes
 */
#define	C_ALPHA	0x01		/* a-z_A-Z */
#define	C_DIGIT	0x02		/* 0-9 */
#define	C_LEX1	0x04		/* \0 \t\n|&;<>() */
#define	C_VAR1	0x08		/* *@#!$-? */
#define	C_SUBOP	0x40		/* "=-+?#%" */
#define	C_IFS	0x80		/* $IFS */

extern	char ctypes [];
#if 0
void	initctypes ARGS((void));
void	setctypes ARGS((const char*, int type));
#endif
#define	ctype(c, t)	!!(ctypes[(unsigned char)(c)]&(t))
#define	letter(c)	ctype(c, C_ALPHA)
#define	digit(c)	ctype(c, C_DIGIT)
#define	letnum(c)	ctype(c, C_ALPHA|C_DIGIT)

#include "table.h"
#include "tree.h"
#include "lex.h"
#include "proto.h"
  
/*
 * 91-07-06 <sjg@sun0>
 * use my simple debug tracing... 
 */
#include "trace.h"

#ifndef fileno
#define fileno(p)	((p)->_fileno)
#endif
