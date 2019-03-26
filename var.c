#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: var.c,v 1.2 1994/05/19 18:32:40 michael Exp michael $";
#endif

#include "sh.h"
#include "ksh_time.h"
#include "ksh_limval.h"
#include "ksh_stat.h"
#include "expand.h"
#include <ctype.h>

/*
 * Variables
 *
 * WARNING: unreadable code, needs a rewrite
 *
 * if (flag&INTEGER), val.i contains integer value, and type contains base.
 * otherwise, (val.s + type) contains string value.
 * if (flag&EXPORT), val.s contains "name=value" for E-Z exporting.
 */
char	null []	= "";
static	struct tbl vtemp;
static char	*formatstr	ARGS((struct tbl *vp, char *s));
static void	export		ARGS((struct tbl *vp, char *val));
static int	special		ARGS((char *name));
static void	getspec		ARGS((struct tbl *vp));
static void	setspec		ARGS((struct tbl *vp));
static void	unsetspec	ARGS((struct tbl *vp));
static struct tbl *arraysearch  ARGS((struct tbl *, int));

/*
 * create a new block for function calls and simple commands
 * assume caller has allocated and set up e->loc
 */
void
newblock()
{
	register struct block *l;
	static char *empty[] = {""};

	l = (struct block *) alloc(sizeof(struct block), ATEMP);
	ainit(&l->area);
	l->argc = 0;
	l->argv = empty;
	l->exit = l->error = NULL;
	tinit(&l->vars, &l->area);
	tinit(&l->funs, &l->area);
	l->next = e->loc;
	e->loc = l;
}

/*
 * pop a block handling special variables
 */
void
popblock()
{
	register struct block *l = e->loc;
	register struct tbl *vp, **vpp = l->vars.tbls, *vq;
	register int i;

	e->loc = l->next;	/* pop block */
	for (i = l->vars.size; --i >= 0; )
		if ((vp = *vpp++) != NULL && (vp->flag&SPECIAL))
			if ((vq = global(vp->name))->flag & ISSET)
				setspec(vq);
			else
				unsetspec(vq);
	afreeall(&l->area);
	afree(l, ATEMP);
}

/*
 * Search for variable, if not found create globally.
 */
struct tbl *
global(n)
	register char *n;
{
	register struct block *l = e->loc;
	register struct tbl *vp;
	register int c;
	unsigned h; 
	int	 array = 0, UNINITIALIZED(val), len;
	char	 *p;

	/* Check to see if this is an array */
	p = skip_varname(n, FALSE);
	if (p != n && *p == '[' && (len = array_ref_len(p))) {
		char *sub, *tmp;

		/* Calculate the value of the subscript */
		array = 1;
		tmp = strnsave(p+1, len-2, ATEMP);
		sub = substitute(tmp, 0);
		afree(tmp, ATEMP);
		n = strnsave(n, p - n, ATEMP);
		val = evaluate(sub);
		if (val < 0 || val > ARRAYMAX)
			errorf("%s: subscript out of range\n", n);
		afree(sub, ATEMP);
	}
	h = hash(n);
	c = n[0];
	if (digit(c)) {
		if (array)
			errorf("bad substitution\n");
		vp = &vtemp;
		vp->flag = (DEFINED|RDONLY);
		vp->type = 0;
		vp->areap = ATEMP;
		*vp->name = c;	/* should strncpy */
		for (c = 0; digit(*n); n++)
			c = c*10 + *n-'0';
		if (c <= l->argc)
			setstr(vp, l->argv[c]);
		return vp;
	}
	if (!letter(c)) {
		if (array)
			errorf("bad substitution\n");
		vp = &vtemp;
		vp->flag = (DEFINED|RDONLY);
		vp->type = 0;
		vp->areap = ATEMP;
		*vp->name = c;
		if (n[1] != '\0')
			return vp;
		vp->flag |= ISSET|INTEGER;
		switch (c) {
		  case '$':
			vp->val.i = kshpid;
			break;
		  case '!':
			/* If no job, expand to nothing */
			if ((vp->val.i = j_async()) == 0)
				vp->flag &= ~(ISSET|INTEGER);
			break;
		  case '?':
			vp->val.i = exstat;
			break;
		  case '#':
			vp->val.i = l->argc;
			break;
		  case '-':
			vp->flag &= ~INTEGER;
			vp->val.s = getoptions();
			break;
		  default:
			vp->flag &= ~(ISSET|INTEGER);
		}
		return vp;
	}
	for (l = e->loc; l != NULL; l = l->next) {
		vp = tsearch(&l->vars, n, h);
		if (vp != NULL)
			if (array)
				return arraysearch(vp, val);
			else
				return vp;
		if (l->next == NULL)
			break;
	}
	vp = tenter(&l->vars, n, h);
	if (array)
		vp = arraysearch(vp, val);
	vp->flag |= DEFINED;
	if (special(n))
		vp->flag |= SPECIAL;
	return vp;
}

/*
 * Search for local variable, if not found create locally.
 */
struct tbl *
local(n)
	register char *n;
{
	register struct block *l = e->loc;
	register struct tbl *vp;
	unsigned h;
	int	 array = 0, UNINITIALIZED(val), len;
	char	 *p;

	/* Check to see if this is an array */
	p = skip_varname(n, FALSE);
	if (p != n && *p == '[' && (len = array_ref_len(p))) {
		char *sub, *tmp;

		/* Calculate the value of the subscript */
		array = 1;
		tmp = strnsave(p+1, len-2, ATEMP);
		sub = substitute(tmp, 0);
		afree(tmp, ATEMP);
		n = strnsave(n, p - n, ATEMP);
		val = evaluate(sub);
		if (val < 0 || val > ARRAYMAX)
			errorf("%s: subscript out of range\n", n);
		afree(sub, ATEMP);
	}
	h = hash(n);
	if (!letter(*n)) {
		vp = &vtemp;
		vp->flag = (DEFINED|RDONLY);
		vp->type = 0;
		vp->areap = ATEMP;
		return vp;
	}
	vp = tenter(&l->vars, n, h);
	if (array)
		vp = arraysearch(vp, val);
	vp->flag |= DEFINED;
	if (special(n))
		vp->flag |= SPECIAL;
	return vp;
}

/* get variable string value */
char *
strval(vp)
	register struct tbl *vp;
{
	char *s;

	if ((vp->flag&SPECIAL))
		getspec(vp);
	if (!(vp->flag&ISSET))
		return null;	/* special to dollar() */
	if (!(vp->flag&INTEGER))	/* string source */
		s = vp->val.s + vp->type;
	else {				/* integer source */
		/* worst case number length is when base=2, so use BITS(long) */
			     /* minus base #     number    null */
		static char strbuf[1 + 2 + 1 + BITS(long) + 1];
		char *digits = (vp->flag & UCASEV_AL) ?
				  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				: "0123456789abcdefghijklmnopqrstuvwxyz";
		register unsigned long n;
		register int base;

		s = strbuf + sizeof(strbuf);
		if (vp->flag & INT_U)
			n = (unsigned long) vp->val.i;
		else
			n = (vp->val.i < 0) ? -vp->val.i : vp->val.i;
		base = (vp->type == 0) ? 10 : vp->type;

		*--s = '\0';
		do {
			*--s = digits[n % base];
			n /= base;
		} while (n != 0);
		if (base != 10) {
			*--s = '#';
			*--s = digits[base % 10];
			if (base >= 10)
				*--s = digits[base / 10];
		}
		if (!(vp->flag & INT_U) && vp->val.i < 0)
			*--s = '-';
		if (vp->flag & (RJUST|LJUST)) /* case already dealt with */
			s = formatstr(vp, s);
	}
	return s;
}

/* get variable integer value, with error checking */
long
intval(vp)
	register struct tbl *vp;
{
	long num;
	int base;

	base = getint(vp, &num);
	if (base == -1)
		errorf("%s: bad number\n", strval(vp));
	return num;
}

/* set variable to string value */
void
setstr(vq, s)
	register struct tbl *vq;
	char *s;
{
	if (!(vq->flag&INTEGER)) { /* string dest */
		if ((vq->flag&ALLOC))
			afree((void*)vq->val.s, vq->areap);
		vq->flag &= ~(ISSET|ALLOC);
		vq->type = 0;
		if (s && (vq->flag & (UCASEV_AL|LCASEV|LJUST|RJUST)))
			s = formatstr(vq, s);
		if ((vq->flag&EXPORT))
			export(vq, s);
		else {
			vq->val.s = strsave(s, vq->areap);
			if (vq->val.s)		/* <sjg> don't lie */
				vq->flag |= ALLOC;
		}
	} else			/* integer dest */
		v_evaluate(vq, s);
	vq->flag |= ISSET;
	if ((vq->flag&SPECIAL))
		setspec(vq);
}

/* set variable to integer */
void
setint(vq, n)
	register struct tbl *vq;
	long n;
{
	if (!(vq->flag&INTEGER)) {
		register struct tbl *vp = &vtemp;
		vp->flag = (ISSET|INTEGER);
		vp->type = 0;
		vp->areap = ATEMP;
		vp->val.i = n;
		setstr(vq, strval(vp));
	} else
		vq->val.i = n;
	vq->flag |= ISSET;
	if ((vq->flag&SPECIAL))
		setspec(vq);
}

int
getint(vp, nump)
	struct tbl *vp;
	long *nump;
{
	register char *s;
	register int c;
	int base, neg;
	long num;
	
	if (vp->flag&SPECIAL)
		getspec(vp);
	/* XXX is it possible for ISSET to be set and val.s to be 0? */
	if (!(vp->flag&ISSET) || (!(vp->flag&INTEGER) && vp->val.s == NULL))
		return -1;
	if (vp->flag&INTEGER) {
		*nump = vp->val.i;
		return vp->type;
	}
	s = vp->val.s + vp->type;
	if (s == NULL)	/* redundent given initial test */
		s = null;
	base = 10;
	num = 0;
	neg = 0;
	for (c = *s++; c ; c = *s++) {
		if (c == '-') {
			neg++;
		} else if (c == '#') {
			base = (int) num;
			if (base < 2 || base > 36)
				return -1;
			num = 0;
		} else if (letnum(c)) {
			if (isdigit(c))
				c -= '0';
			else if (islower(c))
				c -= 'a' - 10; /* todo: assumes ascii */
			else if (isupper(c))
				c -= 'A' - 10; /* todo: assumes ascii */
			else
				c = -1; /* _: force error */
			if (c < 0 || c >= base)
				return -1;
			num = num * base + c;
		} else
			return -1;
	}
	if (neg)
		num = -num;
	*nump = num;
	return base;
}

/* convert variable vq to integer variable, setting its value from vp
 * (vq and vp may be the same)
 */
struct tbl *
strint(vq, vp)
	register struct tbl *vq, *vp;
{
	int base;
	long num;
	
	if ((base = getint(vp, &num)) == -1)
		return NULL;
	if (!(vq->flag & INTEGER) && (vq->flag & ALLOC)) {
		vq->flag &= ~ALLOC;
		afree(vq->val.s, vq->areap);
	}
	vq->val.i = num;
	if (vq->type == 0) /* default base */
		vq->type = base;
	vq->flag |= ISSET|INTEGER;
	if (vq->flag&SPECIAL)
		setspec(vq);
	return vq;
}

static char *
formatstr(vp, s)
	struct tbl *vp;
	char *s;
{
	int olen, nlen;
	char *p;

	olen = strlen(s);

	if (vp->flag & (RJUST|LJUST)) {
		if (!vp->field)	/* default field width */
			vp->field = olen;
		nlen = vp->field;
	} else
		nlen = olen;

	p = (char *) alloc(nlen + 1, ATEMP);
	if (vp->flag & (RJUST|LJUST)) {
		char *fmt;

		if (vp->flag & RJUST) {
			char *q = s + olen;
			/* strip trailing spaces (at&t ksh uses q[-1] == ' ') */
			while (q > s && isspace(q[-1]))
				--q;
			*q = '\0';
			if (q - s > vp->field)
				s += (q - s) - vp->field;
			fmt = ((vp->flag & ZEROFIL) && digit(*s)) ?
				  "%0*.*s"
				: "%*.*s";
		} else {
			/* strip leading spaces/zeros */
			while (isspace(*s))
				s++;
			if (vp->flag & ZEROFIL)
				while (*s == '0')
					s++;
			fmt = "%-*.*s";
		}
		shf_snprintf(p, nlen + 1, fmt,
			vp->field, vp->field, s);
	} else
		memcpy(p, s, olen + 1);

	if (vp->flag & UCASEV_AL) {
		for (s = p; *s; s++)
			if (islower(*s))
				*s = toupper(*s);
	} else if (vp->flag & LCASEV) {
		for (s = p; *s; s++)
			if (isupper(*s))
				*s = tolower(*s);
	}

	return p;
}

/* set variable from enviroment */
int
import(thing)
	char *thing;
{
	register struct tbl *vp;
	register char *tvar, *val;

	val = strchr(thing, '=');
	if (val == NULL)
		return 0;
	tvar = strnsave(thing, val++ - thing, ATEMP);
	vp = local(tvar);
	afree(tvar, ATEMP);
	vp->flag |= DEFINED|ISSET|EXPORT;
	vp->val.s = thing;
	vp->type = val - thing;
	if ((vp->flag&SPECIAL))
		setspec(vp);
	return 1;
}

/*
 * make vp->val.s be "name=value" for quick exporting.
 */
static void
export(vp, val)
	register struct tbl *vp;
	char *val;
{
	register char *xp;
	char *op = (vp->flag&ALLOC) ? vp->val.s : NULL;
	int namelen = strlen(vp->name);
	int vallen = strlen(val) + 1;

	vp->flag |= ALLOC;
	xp = (char*)alloc(namelen + 1 + vallen, vp->areap);
	memcpy(vp->val.s = xp, vp->name, namelen);
	xp += namelen;
	*xp++ = '=';
	vp->type = xp - vp->val.s; /* offset to value */
	memcpy(xp, val, vallen);
	if (op != NULL)
		afree((void*)op, vp->areap);
}

/*
 * lookup variable (according to (set&LOCAL)),
 * set its attributes (INTEGER, RDONLY, EXPORT, TRACE, LJUST, RJUST, ZEROFIL,
 * LCASEV, UCASEV_AL), and optionally set its value if an assignment.
 */
struct tbl *
typeset(var, set, clr, field, base)
	register char *var;
	int clr, set;
	int field, base;
{
	register struct tbl *vp;
	struct tbl *vpbase, *t;
	register char *tvar, *val;

	/* check for valid variable name, search for value */
	val = skip_varname(var, FALSE);
	if (val == var)
		return NULL;
	if (*val == '[') {
		int len = array_ref_len(val);

		if (len == 0)
			return NULL;
		val += len;
	}
	if (*val == '=')
		tvar = strnsave(var, val++ - var, ATEMP);
	else {
		tvar = var;
		val = NULL;
	}
	vp = (set&LOCAL) ? local(tvar) : global(tvar);
	set &= ~LOCAL;
	if (val)
		afree(tvar, ATEMP);

	vpbase = (vp->flag & ARRAY) ? global(basename(var)) : vp;

	/* only allow export flag to be changed (debatable if export should
	 * be clearable).  at&t ksh allows any attribute to be changed, which
	 * means it can be truncated or modified (-L/-R/-Z/-i).
	 */
	if ((vpbase->flag&RDONLY)
	    && (val || (clr & ~EXPORT) || (set & ~EXPORT)))
		errorf("cannot set readonly %s\n", var);

	/* most calls are with set/clr == 0 */
	if (set | clr) {
		/* XXX if x[0] isn't set, there will be problems: need to have
		 * one copy of attributes for arrays...
		 */
		for (t = vpbase; t; t = t->array) {
			int fake_assign;
			char UNINITIALIZED(*s);
			int UNINITIALIZED(aflag);

			fake_assign = (t->flag & ISSET) && (!val || t != vp)
				      && ((set & (UCASEV_AL|LCASEV|LJUST|RJUST|ZEROFIL))
					  || ((t->flag & INTEGER) && (clr & INTEGER))
					  || (!(t->flag & INTEGER) && (set & INTEGER)));
			if (fake_assign) {
				if (t->flag & INTEGER)
					s = strval(t);
				else
					s = t->val.s + t->type;
				aflag = t->flag & ALLOC;
				t->flag &= ~ALLOC;
			}
			if (!(t->flag & INTEGER) && (set & INTEGER)) {
				t->type = 0;
				t->flag &= ~ALLOC;
			}
			t->flag = (t->flag | set) & ~clr;
			/* Don't change base if assignment is to be done,
			 * in case assignment fails.
			 */
			if ((set & INTEGER) && base > 0 && (!val || t != vp))
				t->type = base;
			if (set & (LJUST|RJUST|ZEROFIL))
				t->field = field;
			if (fake_assign) {
				setstr(t, s);
				if (aflag)
					afree((void *) s, t->areap);
			}
		}
	}

	if (val != NULL) {
		if (vp->flag&INTEGER) {
			/* do not zero base before assignment */
			setstr(vp, val);
			/* Done after assignment to override default */
			if (base > 0)
				vp->type = base;
		} else
			setstr(vp, val);
	}

	/* only x[0] is ever exported, so use vpbase */
	if ((vpbase->flag&EXPORT) && !(vpbase->flag&INTEGER)
	    && vpbase->type == 0)
		export(vpbase, (vpbase->flag&ISSET) ? vpbase->val.s : null);

	return vp;
}

void
unset(vp)
	register struct tbl *vp;
{
	if ((vp->flag&ALLOC))
		afree((void*)vp->val.s, vp->areap);
	vp->flag &= SPECIAL;	/* Should ``unspecial'' some vars */
	if (vp->flag & SPECIAL)
		unsetspec(vp);
}

/* return a pointer to the first char past a legal variable name (returns the
 * argument if there is no legal name, returns * a pointer to the terminating
 * null if whole string is legal).
 */
char *
skip_varname(s, aok)
	char *s;
	int aok;
{
	int alen;

	if (!s || !letter(*s))
		return s;
	while (*++s && letnum(*s))
		;
	if (aok && *s == '[' && (alen = array_ref_len(s)))
		s += alen;
	return s;
}

/* Return a pointer to the first character past any legal variable name.  */
char *
skip_wdvarname(s, aok)
	char *s;
	int aok;	/* skip array de-reference? */
{
	if (s[0] == CHAR && letter(s[1])) {
		do
			s += 2;
		while (s[0] == CHAR && letnum(s[1]));
		if (aok) {
			/* skip possible array de-reference */
			char *p = s;
			char c;
			int depth = 0;

			while (1) {
				if (p[0] != CHAR)
					break;
				c = p[1];
				p += 2;
				if (c == '[')
					depth++;
				else if (c == ']' && --depth == 0) {
					s = p;
					break;
				}
			}
		}
	}
	return s;
}

/* Check if coded string s is a variable name */
int
is_wdvarname(s, aok)
	char *s;
	int aok;
{
	char *p = skip_wdvarname(s, aok);

	return p != s && p[0] == EOS;
}

/* Check if coded string s is a variable assignment */
int
is_wdvarassign(s)
	char *s;
{
	char *p = skip_wdvarname(s, TRUE);

	return p != s && p[0] == CHAR && p[1] == '=';
}

/*
 * Make the exported environment from the exported names in the dictionary.
 */
char **
makenv()
{
	struct block *l = e->loc;
	XPtrV env;
	register struct tbl *vp, **vpp;
	register int i;

	XPinit(env, 64);
	for (l = e->loc; l != NULL; l = l->next)
		for (vpp = l->vars.tbls, i = l->vars.size; --i >= 0; )
			if ((vp = *vpp++) != NULL
			    && (vp->flag&(ISSET|EXPORT)) == (ISSET|EXPORT)) {
				register struct block *l2;
				register struct tbl *vp2;
				unsigned h = hash(vp->name);

				/* unexport any redefined instances */
				for (l2 = l->next; l2 != NULL; l2 = l2->next) {
					vp2 = tsearch(&l2->vars, vp->name, h);
					if (vp2 != NULL)
						vp2->flag &= ~EXPORT;
				}
				if ((vp->flag&INTEGER)) {
					/* integer to string */
					char *val;
					val = strval(vp);
					vp->flag &= ~INTEGER;
					setstr(vp, val);
				}
				XPput(env, vp->val.s);
			}
	XPput(env, NULL);
	return (char **) XPclose(env);
}

/*
 * handle special variables with side effects - PATH, SECONDS.
 */
#define STREQ(a, b) ((*a) == (*b) && strcmp((a), (b)) == 0)
static int
special(name)
	register char * name;
{
	static struct {
		char *name;
		int v;
	} names[] = {
			{ "PATH",		V_PATH },
			{ "IFS",		V_IFS },
			{ "SECONDS",		V_SECONDS },
			{ "OPTIND",		V_OPTIND },
			{ "MAIL",		V_MAIL },
			{ "MAILPATH",		V_MAILPATH },
			{ "MAILCHECK",		V_MAILCHECK },
			{ "RANDOM",		V_RANDOM },
			{ "HISTSIZE",		V_HISTSIZE },
			{ "HISTFILE",		V_HISTFILE },
			{ "VISUAL",		V_VISUAL },
			{ "EDITOR",		V_EDITOR },
			{ "COLUMNS",		V_COLUMNS },
			{ "POSIXLY_CORRECT",	V_POSIXLY_CORRECT },
			{ "TMOUT",		V_TMOUT },
			{ "TMPDIR",		V_TMPDIR },
			{ (char *) 0,	0 }
		};
	int i;

	for (i = 0; names[i].name; i++)
		if (STREQ(names[i].name, name))
			return names[i].v;
	return V_NONE;
}

static	time_t	seconds;		/* time SECONDS last set */

static void
getspec(vp)
	register struct tbl *vp;
{
	switch (special(vp->name)) {
	  case V_SECONDS:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) (time((time_t *)0) - seconds));
		vp->flag |= SPECIAL;
		break;
	  case V_RANDOM:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) (rand() & 0x7fff));
		vp->flag |= SPECIAL;
		break;
	  case V_HISTSIZE:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) histsize);
		vp->flag |= SPECIAL;
		break;
	}
}

static void
setspec(vp)
	register struct tbl *vp;
{
	extern void	mbset(), mpset();
	char *s;

	switch (special(vp->name)) {
	  case V_PATH:
		path = strval(vp);
		flushcom(1);	/* clear tracked aliases */
		break;
	  case V_IFS:
		setctypes(s = strval(vp), C_IFS);
		ifs0 = *s;
		break;
	  case V_SECONDS:
		seconds = time((time_t *)0);
		break;
	  case V_OPTIND:
		getopts_reset((int) intval(vp));
		break;
	  case V_MAIL:
		mbset(strval(vp));
		break;
	  case V_MAILPATH:
		mpset(strval(vp));
		break;
	  case V_MAILCHECK:
		/* mail_check_set(intval(vp)); */
		break;
	  case V_RANDOM:
		vp->flag &= ~SPECIAL;
		srand((unsigned int)intval(vp));
		vp->flag |= SPECIAL;
		break;
	  case V_HISTSIZE:
		vp->flag &= ~SPECIAL;
		sethistsize((int) intval(vp));
		vp->flag |= SPECIAL;
		break;
	  case V_HISTFILE:
		sethistfile(strval(vp));
		break;
	  case V_VISUAL:
		set_editmode(strval(vp));
		break;
	  case V_EDITOR:
		if (!(global("VISUAL")->flag & ISSET))
			set_editmode(strval(vp));
		break;
	  case V_COLUMNS:
#ifdef EDIT
		if ((x_cols = intval(vp)) <= 0)
			x_cols = 16;
#endif /* EDIT */
		break;
	  case V_POSIXLY_CORRECT:
		change_flag(FPOSIX, OF_SPECIAL, 1);
		break;
	  case V_TMOUT:
		/* at&t ksh seems to do this (only listen if integer) */
		if (vp->flag & INTEGER)
			ksh_tmout = vp->val.i >= 0 ? vp->val.i : 0;
		break;
	  case V_TMPDIR:
		if (tmpdir) {
			afree(tmpdir, APERM);
			tmpdir = (char *) 0;
		}
		/* Use tmpdir iff it is an absolute path, is writable and
		 * searchable and is a directory...
		 */
		{
			struct stat statb;
			if ((s = strval(vp))[0] == '/'
			    && eaccess(s, W_OK|X_OK) == 0
			    && stat(s, &statb) == 0 && S_ISDIR(statb.st_mode))
				tmpdir = strsave(s, APERM);
		}
		break;
	}
}

static void
unsetspec(vp)
	register struct tbl *vp;
{
	extern void	mbset(), mpset();

	switch (special(vp->name)) {
	  case V_PATH:
		path = def_path;
		flushcom(1);	/* clear tracked aliases */
		break;
	  case V_IFS:
		setctypes(" \t\n", C_IFS);
		ifs0 = ' ';
		break;
	  case V_MAIL:
		mbset((char *) 0);
		break;
	  case V_MAILPATH:
		mpset((char *) 0);
		break;
	  case V_TMOUT:
		/* at&t ksh doesn't do this. TMOUT becomes unspecial so
		 * future assignments don't have effect.  Could be
		 * useful (eg, after "TMOUT=60; unset TMOUT", user
		 * can't get rid of the timeout...).  Should be handled
		 * by generic unset code...
		 */
		ksh_tmout = 0;
		break;
	  case V_TMPDIR:
		/* should not become unspecial */
		if (tmpdir) {
			afree(tmpdir, APERM);
			tmpdir = (char *) 0;
		}
		break;
	  /* todo: generic action for specials (at&t says variables
	   * loose their special meaning when unset but global() checks
	   * the name of new vars to see if they are special)
	   * 	loose meaning: SECONDS, RANDOM, COLUMNS
	   *	unknown: OPTIND, MAIL, MAILPATH, MAILCHECK, HISTSIZE, HISTFILE,
	   *		VISUAL, EDITOR
	   *    no effect: POSIXLY_CORRECT (use set +o posix instead)
	   */
	}
}

/*
 * Search for (and possibly create) a table entry starting with
 * vp, indexed by val.
 */
static struct tbl *
arraysearch(vp, val)
	struct tbl *vp;
	int val;
{
	struct tbl *prev, *curr, *new;

	vp->flag |= ARRAY|DEFINED;

	/* The table entry is always [0] */
	if (val == 0) {
		vp->index = 0;
		return vp;
	}
	prev = vp;
	curr = vp->array;
	while (curr && curr->index < val) {
		prev = curr;
		curr = curr->array;
	}
	if (curr && curr->index == val) {
		if (curr->flag&ISSET)
			return curr;
		else
			new = curr;
	} else
		new = (struct tbl *)alloc(sizeof(struct tbl)+strlen(vp->name)+1, vp->areap);
	strcpy(new->name, vp->name);
	new->flag = vp->flag & ~(ALLOC|DEFINED|ISSET|SPECIAL);
	new->type = vp->type;
	new->areap = vp->areap;
	new->field = vp->field;
	new->index = val;
	if (curr != new) {		/* not reusing old array entry */
		prev->array = new;
		new->array = curr;
	}
	return new;
}

/* Return the length of an array reference (eg, [1+2]) - cp is assumed
 * to point to the open bracket.  Returns 0 if there is no matching closing
 * bracket.
 */
int
array_ref_len(cp)
	const char *cp;
{
	const char *s = cp;
	int c;
	int depth = 0;

	while ((c = *s++) && (c != ']' || --depth))
		if (c == '[')
			depth++;
	if (!c)
		return 0;
	return s - cp;
}

/*
 * Make a copy of the base of an array name
 */
char *
basename(str)
	char	*str;
{
	char	*p;

	if ((p = strchr(str, '[')) == 0)
		/* Shouldn't happen, but why worry? */
		return str;

	return strnsave(str, p - str, ATEMP);
}

/* Set (or overwrite, if !reset) the array variable var to the values in vals.
 */
void
set_array(var, reset, vals)
	char *var;
	int reset;
	char **vals;
{
	struct tbl *vp, *vq;
	int i;

	/* to get local array, use "typeset foo; set -A foo" */
	vp = global(var);

	/* Note: at&t ksh allows set -A but not set +A of a read-only var */
	if ((vp->flag&RDONLY))
		errorf("cannot set readonly %s\n", var);
	/* This code is quite non-optimal */
	if (reset)
		/* trash existing values and attributes */
		unset(vp);
	for (i = 0; vals[i]; i++) {
		vq = arraysearch(vp, i);
		setstr(vq, vals[i]);
	}
}
