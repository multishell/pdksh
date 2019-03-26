/*
 * Korn expression evaluation
 */
/*
 * todo: better error handling: if in builtin, should be builtin error, etc.
 * todo: add ++ --
 * todo: recursive variable expansion (y=1;x=y; let x)
    how to deal with allowing:
	i=0
	set -A x 'x[1]' 'x[2]' 'x[3]' 99
	let z=x[i+=1]
	echo $z
	99
    and disallowing:
	x='y[x]'
	let z=x
 */

#include "sh.h"
#include <ctype.h>


/* The order of these enums is constrained by the order of opinfo[] */
enum token {
	/* binary operators */
	O_EQ = 0, O_NE,
	/* assignments are assumed to be in range O_ASN .. O_BORASN */
	O_ASN, O_TIMESASN, O_DIVASN, O_MODASN, O_PLUSASN, O_MINUSASN,
	       O_LSHIFTASN, O_RSHIFTASN, O_BANDASN, O_BXORASN, O_BORASN,
	O_LSHIFT, O_RSHIFT,
	O_LE, O_GE, O_LT, O_GT,
	O_LAND,
	O_LOR,
	O_TIMES, O_DIV, O_MOD,
	O_PLUS, O_MINUS,
	O_BAND,
	O_BXOR,
	O_BOR,
	O_TERN,
	/* things after this aren't used as binary operators */
	/* unary that are not also binaries */
	O_BNOT, O_LNOT,
	/* misc */
	OPEN_PAREN, CLOSE_PAREN, CTERN,
	/* things that don't appear in the opinfo[] table */
	VAR, LIT, END, BAD
    };
#define LAST_BINOP O_TERN
#define IS_ASSIGNOP(op)	((int)(op) >= (int)O_ASN && (int)(op) <= (int)O_BORASN)

enum prec {
	P_PRIMARY = 0,		/* VAR, LIT, (), ~ ! - + */
	P_MULT,			/* * / % */
	P_ADD,			/* + - */
	P_SHIFT,		/* << >> */
	P_RELATION,		/* < <= > >= */
	P_EQUALITY,		/* == != */
	P_BAND,			/* & */
	P_BXOR,			/* ^ */
	P_BOR,			/* | */
	P_LAND,			/* && */
	P_LOR,			/* || */
	P_TERN,			/* ?: */
	P_ASSIGN		/* = *= /= %= += -= <<= >>= &= ^= |= */
    };
#define MAX_PREC	P_ASSIGN

struct opinfo {
	char		name[4];
	int		len;	/* name length */
	enum prec	prec;	/* precidence: lower is higher */
};

/* Tokens in this table must be ordered so the longest are first
 * (eg, += before +).  If you change something, change the order
 * of enum token too.
 */
static const struct opinfo opinfo[] = {
		{ "==",	 2, P_EQUALITY },	/* before = */
		{ "!=",	 2, P_EQUALITY },	/* before ! */
		{ "=",	 1, P_ASSIGN },		/* keep assigns in a block */
		{ "*=",	 2, P_ASSIGN },
		{ "/=",	 2, P_ASSIGN },
		{ "%=",	 2, P_ASSIGN },
		{ "+=",	 2, P_ASSIGN },
		{ "-=",	 2, P_ASSIGN },
		{ "<<=", 3, P_ASSIGN },
		{ ">>=", 3, P_ASSIGN },
		{ "&=",	 2, P_ASSIGN },
		{ "^=",	 2, P_ASSIGN },
		{ "|=",	 2, P_ASSIGN },
		{ "<<",	 2, P_SHIFT },
		{ ">>",	 2, P_SHIFT },
		{ "<=",	 2, P_RELATION },
		{ ">=",	 2, P_RELATION },
		{ "<",	 1, P_RELATION },
		{ ">",	 1, P_RELATION },
		{ "&&",	 2, P_LAND },
		{ "||",	 2, P_LOR },
		{ "*",	 1, P_MULT },
		{ "/",	 1, P_MULT },
		{ "%",	 1, P_MULT },
		{ "+",	 1, P_ADD },
		{ "-",	 1, P_ADD },
		{ "&",	 1, P_BAND },
		{ "^",	 1, P_BXOR },
		{ "|",	 1, P_BOR },
		{ "?",	 1, P_TERN },
		{ "~",	 1, P_PRIMARY },
		{ "!",	 1, P_PRIMARY },
		{ "(",	 1, P_PRIMARY },
		{ ")",	 1, P_PRIMARY },
		{ ":",	 1, P_PRIMARY },
		{ "",	 0, P_PRIMARY } /* end of table */
	    };


typedef struct expr_state Expr_state;
struct expr_state {
	const char *expression;		/* expression being evaluated */
	const char *tokp;		/* lexical position */
	enum token  tok;		/* token from token() */
	int	    noassign;		/* don't do assignments (for ?:) */
	struct tbl *val;		/* value from token() */
	Expr_state *volatile prev;	/* previous state */
};

enum error_type { ET_UNEXPECTED, ET_BADLIT, ET_BADVAR, ET_STR };

static Expr_state *es;

static void        evalerr  ARGS((enum error_type type, char *str))
						GCC_FUNC_ATTR(noreturn);
static struct tbl *evalexpr ARGS((enum prec prec));
static void        token    ARGS((void));
static struct tbl *tempvar  ARGS((void));
static struct tbl *intvar   ARGS((struct tbl *vp));

/*
 * parse and evalute expression
 */
int
evaluate(expr, rval, error_ok)
	const char *expr;
	long *rval;
	int error_ok;
{
	struct tbl v;
	int ret;

	v.flag = DEFINED|INTEGER;
	v.type = 0;
	ret = v_evaluate(&v, expr, error_ok);
	*rval = v.val.i;
	return ret;
}

/*
 * parse and evalute expression, storing result in vp.
 */
int
v_evaluate(vp, expr, error_ok)
	struct tbl *vp;
	const char *expr;
	volatile int error_ok;
{
	struct tbl *v;
	Expr_state curstate;
	int i;

	/* save state to allow recursive calls */
	curstate.expression = curstate.tokp = expr;
	curstate.noassign = 0;
	curstate.prev = es;
	es = &curstate;

	newenv(E_ERRH);
	if ((i = setjmp(e->jbuf))) {
		quitenv();
		es = curstate.prev;
		if (i == LAEXPR) {
			if (error_ok)
				return 0;
			errorf((char *) 0);
		}
		unwind(i);
		/*NOTREACHED*/
	}

	token();
#if 1 /* ifdef-out to disallow empty expressions to be treated as 0 */
	if (es->tok == END) {
		es->tok = LIT;
		es->val = tempvar();
	}
#endif /* 0 */
	v = intvar(evalexpr(MAX_PREC));

	if (es->tok != END)
		evalerr(ET_UNEXPECTED, (char *) 0);

	if (vp->flag & INTEGER)
		strint(vp, v);
	else
		setstr(vp, strval(v));

	es = curstate.prev;
	quitenv();

	return 1;
}

static void
evalerr(type, str)
	enum error_type type;
	char *str;
{
	char tbuf[2];
	const char *s;

	switch (type) {
	case ET_UNEXPECTED:
		switch (es->tok) {
		case VAR:
			s = es->val->name;
			break;
		case LIT:
			s = strval(es->val);
			break;
		case END:
			s = "end of expression";
			break;
		case BAD:
			tbuf[0] = *es->tokp;
			tbuf[1] = '\0';
			s = tbuf;
			break;
		default:
			s = opinfo[(int)es->tok].name;
		}
		warningf(TRUE, "%s: unexpected `%s'", es->expression, s);
		break;

	case ET_BADLIT:
		warningf(TRUE, "%s: bad number `%s'", es->expression, str);
		break;

	case ET_BADVAR:
		warningf(TRUE, "%s: value of variable `%s' not a number",
			es->expression, str);
		break;

	default: /* keep gcc happy */
	case ET_STR:
		warningf(TRUE, "%s: %s", es->expression, str);
		break;
	}
	unwind(LAEXPR);
}

static struct tbl *
evalexpr(prec)
	enum prec prec;
{
	register struct tbl *vl, UNINITIALIZED(*vr), *vasn;
	register enum token op;
	long UNINITIALIZED(res);

	if (prec == P_PRIMARY) {
		op = es->tok;
		if (op == O_BNOT || op == O_LNOT || op == O_MINUS
		    || op == O_PLUS)
		{
			token();
			vl = intvar(evalexpr(P_PRIMARY));
			if (op == O_BNOT)
				vl->val.i = ~vl->val.i;
			else if (op == O_LNOT)
				vl->val.i = !vl->val.i;
			else if (op == O_MINUS)
				vl->val.i = -vl->val.i;
			/* op == O_PLUS is a no-op */
		} else if (op == OPEN_PAREN) {
			token();
			vl = evalexpr(MAX_PREC);
			if (es->tok != CLOSE_PAREN)
				evalerr(ET_STR, "missing )");
			token();
		} else if (op == VAR || op == LIT) {
			vl = es->val;
			token();
		} else {
			evalerr(ET_UNEXPECTED, (char *) 0);
			/*NOTREACHED*/
		}
		return vl;
	}
	vl = evalexpr(((int) prec) - 1);
	while ((int) (op = es->tok) <= (int) LAST_BINOP && opinfo[(int) op].prec == prec) {
		token();
		vasn = vl;
		if (op != O_ASN) /* vl may not have a value yet */
			vl = intvar(vl);
		if (IS_ASSIGNOP(op)) {
			if (vasn->name[0] == '\0')
				evalerr(ET_STR, "assignment to non-lvalue");
			else if (vasn->flag & RDONLY)
				evalerr(ET_STR,
					"assignment to read only variable");
			vr = intvar(evalexpr(P_ASSIGN));
		} else if (op != O_TERN && op != O_LAND && op != O_LOR)
			vr = intvar(evalexpr(((int) prec) - 1));
		if ((op == O_DIV || op == O_MOD || op == O_DIVASN
		     || op == O_MODASN) && vr->val.i == 0)
		{
			if (es->noassign)
				vr->val.i = 1;
			else
				evalerr(ET_STR, "zero divisor");
		}
		switch ((int) op) {
		case O_TIMES:
		case O_TIMESASN:
			res = vl->val.i * vr->val.i;
			break;
		case O_DIV:
		case O_DIVASN:
			res = vl->val.i / vr->val.i;
			break;
		case O_MOD:
		case O_MODASN:
			res = vl->val.i % vr->val.i;
			break;
		case O_PLUS:
		case O_PLUSASN:
			res = vl->val.i + vr->val.i;
			break;
		case O_MINUS:
		case O_MINUSASN:
			res = vl->val.i - vr->val.i;
			break;
		case O_LSHIFT:
		case O_LSHIFTASN:
			res = vl->val.i << vr->val.i;
			break;
		case O_RSHIFT:
		case O_RSHIFTASN:
			res = vl->val.i >> vr->val.i;
			break;
		case O_LT:
			res = vl->val.i < vr->val.i;
			break;
		case O_LE:
			res = vl->val.i <= vr->val.i;
			break;
		case O_GT:
			res = vl->val.i > vr->val.i;
			break;
		case O_GE:
			res = vl->val.i >= vr->val.i;
			break;
		case O_EQ:
			res = vl->val.i == vr->val.i;
			break;
		case O_NE:
			res = vl->val.i != vr->val.i;
			break;
		case O_BAND:
		case O_BANDASN:
			res = vl->val.i & vr->val.i;
			break;
		case O_BXOR:
		case O_BXORASN:
			res = vl->val.i ^ vr->val.i;
			break;
		case O_BOR:
		case O_BORASN:
			res = vl->val.i | vr->val.i;
			break;
		case O_LAND:
			if (!vl->val.i)
				es->noassign++;
			vr = intvar(evalexpr(((int) prec) - 1));
			res = vl->val.i && vr->val.i;
			if (!vl->val.i)
				es->noassign--;
			break;
		case O_LOR:
			if (vl->val.i)
				es->noassign++;
			vr = intvar(evalexpr(((int) prec) - 1));
			res = vl->val.i || vr->val.i;
			if (vl->val.i)
				es->noassign--;
			break;
		case O_TERN:
			{
				int e = vl->val.i != 0;
				if (!e)
					es->noassign++;
				vl = evalexpr(MAX_PREC);
				if (!e)
					es->noassign--;
				if (es->tok != CTERN)
					evalerr(ET_STR, "missing :");
				token();
				if (e)
					es->noassign++;
				vr = evalexpr(MAX_PREC);
				if (e)
					es->noassign--;
				vl = e ? vl : vr;
			}
			break;
		case O_ASN:
			res = vr->val.i;
			break;
		}
		if (IS_ASSIGNOP(op)) {
			vr->val.i = res;
			if (vasn->flag & INTEGER)
				strint(vasn, vr);
			else
				setstr(vasn, strval(vr));
			vl = vr;
		} else if (op != O_TERN)
			vl->val.i = res;
	}
	return vl;
}

static void
token()
{
	register const char *cp;
	register int c;
	char *tvar;

	/* skip white space */
	for (cp = es->tokp; (c = *cp), isspace(c); cp++)
		;
	es->tokp = cp;

	if (c == '\0')
		es->tok = END;
	else if (letter(c)) {
		for (; letnum(c); c = *cp++)
			;
		if (c == '[') {
			int len;

			len = array_ref_len(cp - 1);
			if (len == 0)
				evalerr(ET_STR, "missing ]");
			cp += len;
		}
		if (es->noassign)
			es->val = tempvar();
		else {
			tvar = strnsave(es->tokp, --cp - es->tokp, ATEMP);
			es->val = global(tvar);
			afree(tvar, ATEMP);
		}
		es->tok = VAR;
	} else if (digit(c)) {
		for (; c != '_' && (letnum(c) || c == '#'); c = *cp++)
			;
		tvar = strnsave(es->tokp, --cp - es->tokp, ATEMP);
		es->val = tempvar();
		es->val->flag &= ~INTEGER;
		es->val->type = 0;
		es->val->val.s = tvar;
		if (strint(es->val, es->val) == NULL)
			evalerr(ET_BADLIT, tvar);
		afree(tvar, ATEMP);
		es->tok = LIT;
	} else {
		int i, n0;

		for (i = 0; (n0 = opinfo[i].name[0]); i++)
			if (c == n0
			    && strncmp(cp, opinfo[i].name, opinfo[i].len) == 0)
			{
				es->tok = (enum token) i;
				cp += opinfo[i].len;
				break;
			}
		if (!n0)
			es->tok = BAD;
	}
	es->tokp = cp;
}

static struct tbl *
tempvar()
{
	register struct tbl *vp;

	vp = (struct tbl*) alloc(sizeof(struct tbl), ATEMP);
	vp->flag = ISSET|INTEGER;
	vp->type = 0;
	vp->areap = ATEMP;
	vp->val.i = 0;
	vp->name[0] = '\0';
	return vp;
}

/* cast (string) variable to temporary integer variable */
static struct tbl *
intvar(vp)
	register struct tbl *vp;
{
	register struct tbl *vq;

	/* try to avoid replacing a temp var with another temp var */
	if (vp->name[0] == '\0'
	    && (vp->flag & (ISSET|INTEGER)) == (ISSET|INTEGER))
		return vp;

	vq = tempvar();
	vq->type = 0;
	if (strint(vq, vp) == NULL) {
		evalerr(ET_BADVAR, vp->name);
		/*
		if ((vp->flag&ISSET) && vp->val.s && *(vp->val.s)) {
			evalerr("bad number");
		} else {
			vq->flag |= (ISSET|INTEGER);
			vq->type = 10;
			vq->val.i = 0;
		}
		*/
	}
	return vq;
}
