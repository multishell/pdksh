/*
 * test(1); version 7-like  --  author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by Michael Rendell to add Korn's [[ .. ]] expressions.
 * modified by J.T. Conklin to add POSIX compatibility.
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: c_test.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "ksh_stat.h"

/* test(1) accepts the following grammar:
	oexpr	::= aexpr | aexpr "-o" oexpr ;
	aexpr	::= nexpr | nexpr "-a" aexpr ;
	nexpr	::= primary | "!" nexpr ;
	primary	::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" oexpr ")"
		;

	unary-operator ::= "-a"|"-r"|"-w"|"-x"|"-e"|"-f"|"-d"|"-c"|"-b"|"-p"|
			   "-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|
			   "-L"|"-h"|"-S";

	binary-operator ::= "="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			    "-nt"|"-ot"|"-ef"|
			    "<"|">"	# rules used for [[ .. ]] expressions
			    ;
	operand ::= <any thing>
*/


#define is_unop(s)	is_op(u_ops, s)
#define is_binop(s)	is_op(b_ops, s)
#define is_not(s)	((s)[0] == '!' && (s)[1] == '\0')
#define is_and(s)	((s)[0] == '-' && (s)[1] == 'a' && (s)[2] == '\0')
#define is_or(s)	((s)[0] == '-' && (s)[1] == 'o' && (s)[2] == '\0')

#define T_ERR_EXIT	2	/* POSIX says > 1 for errors */

/* Various types of operations.  Keeping things grouped nicely
 * (unary,binary) makes switch() statements more efficeint.
 */
enum Op {
	NONOP = 0,	/* non-operator */
	/* unary operators */
	STNZE, STZER, OPTION,
	FILEXST, FILBDEV, FILCDEV, FILID, FILREG, FILGID, FILSETG,
	FILSYM, FILSTCK, FILUID, FILFIFO, FILRD, FILGZ, FILSOCK,
	FILTT, FILSETU, FILWR, FILEX,
	/* binary operators */
	STEQL, STNEQ, STLT, STGT, INTEQ, INTNE, INTGT, INTGE, INTLT, INTLE,
	FILEQ, FILNT, FILOT,
};
typedef enum Op Op;

struct t_op {
	char	op_text[4];
	Op	op_num;
};
static const struct t_op u_ops [] = {
	{"-a",	FILEXST },
	{"-b",	FILBDEV },
	{"-c",	FILCDEV },
	{"-d",	FILID },
	{"-e",	FILEXST },
	{"-f",	FILREG },
	{"-G",	FILGID },
	{"-g",	FILSETG },
	{"-h",	FILSYM },
	{"-k",	FILSTCK },
	{"-L",	FILSYM },
	{"-n",	STNZE },
	{"-O",	FILUID },
	{"-o",	OPTION },
	{"-p",	FILFIFO },
	{"-r",	FILRD },
	{"-s",	FILGZ },
	{"-S",	FILSOCK },
	{"-t",	FILTT },
	{"-u",	FILSETU },
	{"-w",	FILWR },
	{"-x",	FILEX },
	{"-z",	STZER },
	{"",	NONOP }
    };
static const struct t_op b_ops [] = {
	{"=",	STEQL },
	{"!=",	STNEQ },
	{"<",	STLT },
	{">",	STGT },
	{"-eq",	INTEQ },
	{"-ne",	INTNE },
	{"-gt",	INTGT },
	{"-ge",	INTGE },
	{"-lt",	INTLT },
	{"-le",	INTLE },
	{"-ef",	FILEQ },
	{"-nt",	FILNT },
	{"-ot",	FILOT },
	{"",	NONOP }
    };

static char **t_wp;
static int isdbracket;	/* true when evaluating [[ .. ]] expressions */
static int t_error;
static char *arg0;

static void syntax ARGS((char *op, char *msg));
static int oexpr ARGS((void));
static int aexpr ARGS((void));
static int nexpr ARGS((void));
static int primary ARGS((void));
static int eval_unop ARGS((Op op, char *opnd1));
static int eval_binop ARGS((Op op, char *opnd1, char *opnd2));
static Op is_op ARGS((const struct t_op *otab, char *s));

int
c_test(wp)
	char **wp;
{
	Op	op;
	int	argc;
	int	res;

	t_error = 0;
	arg0 = wp[0];

	isdbracket = strcmp(arg0, "[[") == 0;

	for (argc = 0; wp[argc]; argc++)
		;

	if (strcmp(arg0, "[") == 0) {
		if (strcmp(wp[--argc], "]") != 0) {
			syntax(NULL, "missing ]");
			return T_ERR_EXIT;
		}
		wp[argc] = NULL;
	}

	/* 
	 * Handle the special cases from POSIX.2, section 4.62.4.
	 * Implementation of all the rules isn't necessary since 
	 * our parser does the right thing for the ommited steps.
	 */
	if (!isdbracket) {
		char **owp = wp;
		int val, invert = 0;

		switch (argc - 1) {
		  case 4:
			if (!is_not(wp[1]))
				break;
			invert++;
			wp++;
			/* fall through to 3 argument test... */
		  case 3:
			if ((op = is_binop(wp[2]))) {
				t_wp = wp + 2; /* for error message */
				val = eval_binop(op, wp[1], wp[3]);
				if (t_error)
					return T_ERR_EXIT;
				if (invert & 1)
					val = !val;
				return !val;
			}
			if (!is_not(wp[1]))
				break;
			invert++;
			wp++;
			/* fall through to 2 argument test... */
		  case 2:
			if (!is_not(wp[1]))
				/* let parser deal with unary primaries */
				break;
			invert++;
			wp++;
			/* fall through to 1 argument test... */
		  case 1:
			val = *wp[1] != '\0';
			if (invert & 1)
				val = !val;
			return !val;
		  case 0:
			return !0;
		}
		wp = owp;
	}

	t_wp = wp + 1;
	res = oexpr();

	if (!t_error && *t_wp != NULL)
		syntax(*t_wp, "unexpected operator/operand");

	return t_error ? T_ERR_EXIT : !res;
}

/* is_db_*op() are called from syn.c for [[ .. ]] expression */
int
is_db_unop(s)
	char *s;
{
	isdbracket = TRUE;
	return (int) is_unop(s);
}

int
is_db_binop(s)
	char *s;
{
	isdbracket = TRUE;
	return (int) is_binop(s);
}

/* returns true if second argument of binary operator op is a pattern */
int
is_db_patop(op)
	int op;
{
	return (Op) op == STEQL || (Op) op == STNEQ;
}

static void
syntax(op, msg)
	char	*op;
	char	*msg;
{
	t_error++;
	if (op)
		shellf("%s: %s: %s\n", arg0, op, msg);
	else
		shellf("%s: %s\n", arg0, msg);
	shf_flush(shl_out);
}

static int
oexpr()
{
	int res;

	res = aexpr();
	if (!t_error && *t_wp && is_or(*t_wp)) {
		t_wp++;
		return oexpr() || res;
	}
	return res;
}

static int
aexpr()
{
	int res;

	res = nexpr();
	if (!t_error && *t_wp && is_and(*t_wp)) {
		t_wp++;
		return aexpr() && res;
	}
	return res;
}

static int
nexpr()
{
	if (*t_wp && is_not(*t_wp)) {
		t_wp++;
		return !nexpr();
	}
	return primary();
}

static int
primary()
{
	register char *opnd1, *opnd2;
	int res;
	Op op;

	if (*t_wp == NULL) {
		syntax(NULL, "argument expected");
		return 1;
	}
	if (strcmp(*t_wp, "("/*)*/) == 0) {
		t_wp++;
		res = oexpr();
		if (t_error)
			return 1;
		if (*t_wp == NULL || strcmp(*t_wp, /*(*/")") != 0) {
			syntax(*t_wp, "closing paren expected");
			return 1;
		}
		t_wp++;
		return res;
	}
	if (isdbracket && strcmp(*t_wp, "-BE") == 0)
		/* next 3 args form binary expression */
		t_wp++;
	else if ((op = is_unop(*t_wp))) {
		/* unary expression */
		if ((opnd1 = *++t_wp) == NULL) {
			if (op != FILTT) {
				syntax(t_wp[-1], "argument expected");
				return 1;
			}
		} else
			t_wp++;

		return eval_unop(op, opnd1);
	}
	opnd1 = *t_wp++;
	if (*t_wp != NULL && (op = is_binop(*t_wp))) {
		if ((opnd2 = *++t_wp) == NULL) {
			syntax(t_wp[-1], "argument expected");
			return 1;
		}
		t_wp++;

		return eval_binop(op, opnd1, opnd2);
	}
	/* XXX debugging */
	if (isdbracket) {
		syntax(opnd1, "internal error - unexpected [[ ]] token");
		return 1;
	}
	return *opnd1 != '\0';
}

static int
eval_unop(op, opnd1)
	Op op;
	char *opnd1;
{
	int res;
	struct stat s;
	switch ((int) op) {
	  case STNZE:
		return *opnd1 != '\0';
	  case STZER:
		return *opnd1 == '\0';
	  case OPTION:
		res = option(opnd1);
		return res < 0 ? 0 : Flag(res); 
	  case FILRD:
		return eaccess(opnd1, R_OK) == 0;
	  case FILWR:
		return eaccess(opnd1, W_OK) == 0;
	  case FILEX:
		return eaccess(opnd1, X_OK) == 0;
	  case FILEXST:
		return stat(opnd1, &s) == 0;
	  case FILREG:
		return stat(opnd1, &s) == 0 && S_ISREG(s.st_mode);
	  case FILID:
		return stat(opnd1, &s) == 0 && S_ISDIR(s.st_mode);
	  case FILCDEV:
		return stat(opnd1, &s) == 0 && S_ISCHR(s.st_mode);
	  case FILBDEV:
		return stat(opnd1, &s) == 0 && S_ISBLK(s.st_mode);
	  case FILFIFO:
#ifdef S_ISFIFO
		return stat(opnd1, &s) == 0 && S_ISFIFO(s.st_mode);
#else
		return 0;
#endif
	  case FILSYM:
#ifdef S_ISLNK
		return lstat(opnd1, &s) == 0 && S_ISLNK(s.st_mode);
#else
		return 0;
#endif
	  case FILSOCK:
#ifdef S_ISSOCK
		return stat(opnd1, &s) == 0 && S_ISSOCK(s.st_mode);
#else
		return 0;
#endif
	  case FILSETU:
		return stat(opnd1, &s) == 0 && (s.st_mode & S_ISUID) == S_ISUID;
	  case FILSETG:
		return stat(opnd1, &s) == 0 && (s.st_mode & S_ISGID) == S_ISGID;
	  case FILSTCK:
		return stat(opnd1, &s) == 0 && (s.st_mode & S_ISVTX) == S_ISVTX;
	  case FILGZ:
		return stat(opnd1, &s) == 0 && s.st_size > 0L;
	  case FILTT:
		return isatty(opnd1 ? getn(opnd1) : 0);
	  case FILUID:
		return stat(opnd1, &s) == 0 && s.st_uid == geteuid();
	  case FILGID:
		return stat(opnd1, &s) == 0 && s.st_gid == getegid();
	  default:
		syntax(t_wp[-2], "internal error: unknown unop");
		return 1;
	}
}

static int
eval_binop(op, opnd1, opnd2)
	Op op;
	char *opnd1;
	char *opnd2;
{
	struct stat b1, b2;

	switch ((int) op) { /* keep gcc quiet */
	  case STEQL:
		if (isdbracket)
			return gmatch(opnd1, opnd2);
		return strcmp(opnd1, opnd2) == 0;
	  case STNEQ:
		if (isdbracket)
			return !gmatch(opnd1, opnd2);
		return strcmp(opnd1, opnd2) != 0;
	  case STLT:
		return strcmp(opnd1, opnd2) < 0;
	  case STGT:
		return strcmp(opnd1, opnd2) > 0;
	  case INTEQ:
		return evaluate(opnd1) == evaluate(opnd2);
	  case INTNE:
		return evaluate(opnd1) != evaluate(opnd2);
	  case INTGE:
		return evaluate(opnd1) >= evaluate(opnd2);
	  case INTGT:
		return evaluate(opnd1) > evaluate(opnd2);
	  case INTLE:
		return evaluate(opnd1) <= evaluate(opnd2);
	  case INTLT:
		return evaluate(opnd1) < evaluate(opnd2);
	  case FILNT:
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_mtime > b2.st_mtime;
	  case FILOT:
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_mtime < b2.st_mtime;
	  case FILEQ:
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_dev == b2.st_dev
		       && b1.st_ino == b2.st_ino;
	default:
		syntax(t_wp[-2], "internal error: unknown binop");
		return 1;
	}
}

static Op
is_op(otab, s)
	const struct t_op *otab;
	char *s;
{
	char sc1;

	if (*s) {
		sc1 = s[1];
		for (; otab->op_text[0]; otab++)
			if (sc1 == otab->op_text[1]
			    && strcmp(s, otab->op_text) == 0
			    && (isdbracket || (otab->op_num != STLT
					       && otab->op_num != STGT)))
				return otab->op_num;
	}
	return NONOP;
}
