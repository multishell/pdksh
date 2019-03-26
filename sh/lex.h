/*
 * Source input, lexer and parser
 */

/* $Id: lex.h,v 1.2 1992/04/25 08:33:28 sjg Exp $ */

#define	IDENT	64

typedef struct source Source;
struct source {
	char   *str;		/* input pointer */
	int	type;		/* input type */
	union {
		char  **strv;	/* string [] */
		FILE   *file;	/* file */
		struct tbl *tblp; /* alias */
	} u;
	int	line;		/* line number */
	char   *file;		/* input file name */
	int	echo;		/* echo input to shlout */
	Source *next;		/* stacked source */
};

/* Source.type values */
#define	SEOF	0		/* input EOF */
#define	STTY	1		/* terminal input */
#define	SFILE	2		/* file input */
#define	SSTRING	4		/* string */
#define	SWSTR	3		/* string without \n */
#define	SWORDS	5		/* string[] */
#define	SWORDSEP 8		/* string[] seperator */
#define	SALIAS	6		/* alias expansion */
#define	SHIST	7		/* history expansion */

Source *pushs ARGS((int stype)); 	/* push Source */
struct op *compile ARGS((Source *s));	/* compile tree */

/*
 * states while lexing word
 */
#define	SBASE	0		/* outside any lexical constructs */
#define	SWORD	6		/* implicit quoting for substitute() */
#define	SDPAREN	7		/* inside (( )), implicit quoting */
#define	SSQUOTE	1		/* inside '' */
#define	SDQUOTE	2		/* inside "" */
#define	SBRACE	3		/* inside ${} */
#define	SPAREN	4		/* inside $() */
#define	SBQUOTE	5		/* inside `` */

Extern	int	multiline;	/* \n changed to ; */

typedef union {
	int	i;
	char   *cp;
	char  **wp;
	struct op *o;
	struct ioword *iop;
} YYSTYPE;

#define	LWORD	256
#define	LOGAND	257
#define	LOGOR	258
#define	BREAK	259
#define	IF	260
#define	THEN	261
#define	ELSE	262
#define	ELIF	263
#define	FI	264
#define	CASE	265
#define	ESAC	266
#define	FOR	267
#define	WHILE	268
#define	UNTIL	269
#define	DO	270
#define	DONE	271
#define	IN	272
#define	FUNCTION 273
#define	TIME	274
#define	REDIR	275
#define	MPAREN	276		/* () */
#define	MDPAREN	277		/* (( )) */
#define	YYERRCODE 300

/* flags to yylex */
#define	CONTIN	BIT(0)		/* skip new lines to complete command */
#define	ONEWORD	BIT(1)		/* single word for substitute() */
#define	ALIAS	BIT(2)		/* recognize alias */
#define	KEYWORD	BIT(3)		/* recognize keywords */
#define	LETEXPR	BIT(4)		/* get expression inside (( )) */

#define	SYNTAXERR	zzerr()
#define	HERES	10		/* max << in line */

Extern	char	line [LINE+1];	/* input line */
Extern	Source *source;		/* yyparse/yylex source */
Extern	YYSTYPE	yylval;		/* result from yylex */
Extern	int	yynerrs;
Extern	struct ioword *heres [HERES], **herep;
Extern	char	ident [IDENT+1];

extern	int	yylex ARGS((int flags));
extern	void	yyerror ARGS((const char *msg));

#define	HISTORY	100		/* size of saved history */

extern	char   *history [HISTORY];	/* saved commands */
extern	char  **histptr;	/* last history item */
extern	int	histpush;	/* number of pushed fc commands */

extern	char	**histget();
extern  char	**histpos();
extern	int	histnum();
extern	char	*findhist();
extern	int	histN();

#ifdef EDIT

extern	void	x_init ARGS ((void));	/* setup tty modes */
extern	void	x_init_emacs ARGS ((void));
extern	void	x_emacs_keys ();
extern	void	x_bind();

extern	int	x_read ARGS ((int fd, char *buf, size_t len));
extern	int	x_emacs ARGS ((char *buf, size_t len));
extern	int	x_vi ARGS ((char *buf, size_t len));

extern	bool_t	x_mode ARGS ((bool_t));	/* set/clear cbreak mode */
extern	int	x_getc();		/* get tty char */
extern	void	x_flush(), x_putc(), x_puts(); /* put tty char */

extern	int	x_cols;		/* tty columns */

#endif
