/* $Id: table.h,v 1.3 1994/05/31 13:34:34 michael Exp $ */

/*
 * generic hashed associative table for commands and variables.
 */

struct table {
	Area   *areap;		/* area to allocate entries */
	short	size, nfree;	/* hash size (always 2^^n), free entries */
	struct	tbl **tbls;	/* hashed table items */
};

struct tbl {			/* table item */
	INT32	flag;		/* flags */
	int	type;		/* command type (see below), base (if INTEGER),
				 * or offset from val.s of value (if EXPORT) */
	Area	*areap;		/* area to allocate from */
	union {
		char *s;	/* string */
		long i;		/* integer */
		int (*f) ARGS((char **));	/* int function */
		struct op *t;	/* "function" tree */
	} val;			/* value */
	int	index;		/* index for an array */
	int	field;		/* field with for -L/-R/-Z */
	struct tbl *array;	/* array values */
	char	name[4];	/* name -- variable length */
};

/* common flag bits */
#define	ALLOC	BIT(0)		/* val.s has been allocated */
#define	DEFINED	BIT(1)		/* is defined in block */
#define	ISSET	BIT(2)		/* has value, vp->val.[si] */
#define	EXPORT	BIT(3)		/* exported variable/function */
#define	TRACE	BIT(4)		/* var: user flagged, func: execution tracing */
/* (start non-common flags at 8) */
/* flag bits used for variables */
#define	SPECIAL	BIT(8)		/* PATH, IFS, SECONDS, etc */
#define	INTEGER	BIT(9)		/* val.i contains integer value */
#define	RDONLY	BIT(10)		/* read-only variable */
#define	LOCAL	BIT(11)		/* for local typeset() */
#define ARRAY	BIT(13)		/* array */
#define LJUST	BIT(14)		/* left justify */
#define RJUST	BIT(15)		/* right justify */
#define ZEROFIL	BIT(16)		/* 0 filled if RJUSTIFY, strip 0s if LJUSTIFY */
#define LCASEV	BIT(17)		/* convert to lower case */
#define UCASEV_AL BIT(18)	/* convert to upper case / autoload function */
#define INT_U	BIT(19)		/* unsigned integer */
#define INT_L	BIT(20)		/* long integer (no-op) */
/* flag bits used for taliases/builtins/aliases/keywords/functions */
#define KEEPASN	BIT(8)		/* keep command assignments (eg, var=x cmd) */
#define FINUSE	BIT(9)		/* function being executed */
#define FDELETE	BIT(10)		/* function deleted while it was executing */
#define SPEC_BI	BIT(11)		/* a POSIX special builtin */
#define REG_BI	BIT(12)		/* a POSIX regular builtin */

/* command types */
#define	CNONE	0		/* undefined */
#define	CSHELL	1		/* built-in */
#define	CFUNC	2		/* function */
#define	CEXEC	4		/* executable command */
#define	CALIAS	5		/* alias */
#define	CKEYWD	6		/* keyword */
#define CTALIAS	7		/* tracked alias */

/*
 * activation record for function blocks
 */
struct block {
	Area	area;		/* area to allocate things */
	int	argc;		/* current $# */
	char **	argv;		/* current $* */
	struct	table vars;	/* local variables */
	struct	table funs;	/* local functions */
#if 1
	char *	error;		/* error handler */
	char *	exit;		/* exit handler */
#else
	Trap	error, exit;
#endif
	struct	block *next;	/* enclosing block */
};

EXTERN	struct table taliases;	/* tracked aliases */
EXTERN	struct table builtins;	/* built-in commands */
EXTERN	struct table aliases;	/* aliases */
EXTERN	struct table keywords;	/* keywords */
EXTERN	struct table homedirs;	/* homedir() cache */

struct builtin {
	char   *name;
	int  (*func)();
};

/* these really are externs! Look in table.c for them */
extern const struct builtin shbuiltins [], kshbuiltins [];

/* var spec values */
#define	V_NONE			0
#define	V_PATH			1
#define	V_IFS			2
#define	V_SECONDS		3
#define	V_OPTIND		4
#define	V_MAIL			5
#define	V_MAILPATH		6
#define	V_MAILCHECK		7
#define	V_RANDOM		8
#define V_HISTSIZE		9
#define V_HISTFILE		10
#define V_VISUAL		11
#define V_EDITOR		12
#define V_COLUMNS		13
#define V_POSIXLY_CORRECT	14
#define V_TMOUT			15
#define V_TMPDIR		16

/* values for set_prompt() */
#define PS1	0		/* command */
#define PS2	1		/* command continuation */

EXTERN	char   *path;		/* PATH value */
EXTERN	char   *def_path;	/* path to use if PATH not set */
EXTERN	char   *tmpdir;		/* TMPDIR value */
EXTERN	char   *prompt;	
EXTERN	int	cur_prompt;	/* PS1 or PS2 */
