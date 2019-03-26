/*
 * Expansion - quoting, separation, substitution, globbing
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: eval.c,v 1.4 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include <pwd.h>
#include "ksh_dir.h"
#include "expand.h"

/*
 * string expansion
 *
 * first pass: quoting, IFS separation, ${} and $() substitution.
 * second pass: filename expansion (*?[]~).
 */

/* expansion generator state */
typedef struct Expand {
	/* int  type; */	/* see expand() */
	char   *str;		/* string */
	union {
		char  **strv;	/* string[] */
		struct shf *shf;/* file */
	} u;			/* source */
	struct tbl *var;	/* variable in ${var..} */
	short	split;		/* split "$@" / call waitlast $() */
} Expand;

#define	XBASE		0	/* scanning original */
#define	XSUB		1	/* expanding ${} string */
#define	XARGSEP		2	/* ifs0 between "$*" */
#define	XARG		3	/* expanding $*, $@ */
#define	XCOM		4	/* expanding $() */
#define XNULLSUB	5	/* "$@" when $# is 0 (don't generate word) */

/* States used for field splitting */
#define IFS_WORD	0	/* word has chars (or quotes) */
#define IFS_WS		1	/* have seen IFS white-space */
#define IFS_NWS		2	/* have seen IFS non-white-space */

static	int	varsub ARGS((Expand *xp, char *sp, int stype));
static	int	comsub ARGS((Expand *xp, char *cp));
static	char   *trimsub ARGS((char *str, char *pat, int how));
static	void	glob ARGS((char *cp, XPtrV *wp));
static	void	globit ARGS((char *ds, char *dp, char *sp, XPtrV *wp, int check));
static	char   *tilde ARGS((char *acp));
static	char   *homedir ARGS((char *name));
#ifdef BRACEEXPAND
static	void	alt_expand  ARGS((char *cp, XPtrV *wp, int f));
static	int	alt_count   ARGS((char *cp));
static	int	alt_scan    ARGS((char **cpp, char **dpp, int endc, int bal));
#endif

/* compile and expand word */
char *
substitute(cp, f)
	const char *cp;
	int f;
{
	struct source *s, *sold;

	sold = source;
	s = pushs(SWSTR);
	s->str = (char *) cp;
	source = s;
	if (yylex(ONEWORD) != LWORD)
		errorf("eval:substitute error\n");
	source = sold;
	afree(s, ATEMP);
	return evalstr(yylval.cp, f);
}

/*
 * expand arg-list
 */
char **
eval(ap, f)
	register char **ap;
	int f;
{
	XPtrV w;

	if (*ap == NULL)
		return ap;
	XPinit(w, 32);
	XPput(w, NULL);		/* space for shell name */
#ifdef	SHARPBANG
	XPput(w, NULL);		/* and space for one arg */
#endif
	while (*ap != NULL)
		expand(*ap++, &w, f);
	XPput(w, NULL);
#ifdef	SHARPBANG
	return (char **) XPclose(w) + 2;
#else
	return (char **) XPclose(w) + 1;
#endif
}

/*
 * expand string
 */
char *
evalstr(cp, f)
	char *cp;
	int f;
{
	XPtrV w;

	XPinit(w, 1);
	expand(cp, &w, f);
	cp = (XPsize(w) == 0) ? "" : (char*) *XPptrv(w);
	XPfree(w);
	return cp;
}

/*
 * expand string - return only one component
 * used from iosetup to expand redirection files
 */
char *
evalonestr(cp, f)
	register char *cp;
	int f;
{
	XPtrV w;

	XPinit(w, 1);
	expand(cp, &w, f);
	switch (XPsize(w)) {
	case 0:
		cp = "";
		break;
	case 1:
		cp = (char*) *XPptrv(w);
		break;
	default:
		cp = evalstr(cp, f&~DOGLOB);
		break;
	}
	XPfree(w);
	return cp;
}

/* for nested substitution: ${var:=$var2} */
typedef struct SubType {
	short	type;		/* [=+-?%#] action after expanded word */
	short	base;		/* begin position of expanded word */
	struct tbl *var;	/* variable for ${var..} */
	int	quote;		/* saved value of quote (for ${..[%#]..}) */
} SubType;

void
expand(cp, wp, f)
	char *cp;		/* input word */
	register XPtrV *wp;	/* output words */
	int f;			/* DO* flags */
{
	register int UNINITIALIZED(c);
	register int type = XBASE; /* expansion type */
	register int quote = 0;	/* quoted */
	XString ds;		/* destination string */
	register char *dp, *sp;	/* dest., source */
	int fdo, word;		/* second pass flags; have word */
	int doblank;		/* field spliting of parameter/command subst */
	Expand x;		/* expansion variables */
	SubType subtype [10];	/* substitution type stack */
	register SubType *st = subtype + 10;
	int UNINITIALIZED(newlines); /* For trailing newlines in COMSUB */
	int trimming = 0;	/* flag if expanding ${var#pat} or ${var%pat} */
	int oquote;
	char *firsteq = (char *) 0;

	if (cp == NULL)
		errorf("eval:expand(NULL)\n");
	if (Flag(FNOGLOB))
		f &= ~DOGLOB;

#ifdef BRACEEXPAND
	/* look for '{' in the input word. (}) */
	if (Flag(FBRACEEXPAND) && (f & DOGLOB) &&
	    (dp = strchr(cp, '{')) != NULL &&
	    (dp[-1] == CHAR) &&
    	    !(dp[1] == CHAR && dp[2] == '}'))
	{
		alt_expand(cp, wp, f);
		return;
	}
#endif

	Xinit(ds, dp, 128);	/* init dest. string */
	type = XBASE;
	sp = cp;
	fdo = 0;
	doblank = 0;
	word = (f&DOBLANK) ? IFS_WS : IFS_WORD;

	while (1) {
		Xcheck(ds, dp);

		switch (type) {
		  case XBASE:	/* original prefixed string */
			c = *sp++;
			switch (c) {
			  case EOS:
				c = 0;
				break;
			  case CHAR:
				c = *sp++;
				break;
			  case QCHAR:
				quote |= 2; /* temporary quote */
				c = *sp++;
				break;
			  case OQUOTE:
				word = IFS_WORD;
				quote = 1;
				continue;
			  case CQUOTE:
				quote = 0;
				continue;
			  case COMSUB:
				if (f & DONTRUNCOMMAND) {
					word = IFS_WORD;
					*dp++ = '$'; *dp++ = '(';
					while (*sp != '\0') {
						Xcheck(ds, dp);
						*dp++ = *sp++;
					}
					*dp++ = ')';
				} else {
					type = comsub(&x, sp);
					if (type == XCOM && (f&DOBLANK))
						doblank++;
					sp = strchr(sp, 0) + 1;
					newlines = 0;
				}
				continue;
			  case EXPRSUB:
				word = IFS_WORD;
				if (f & DONTRUNCOMMAND) {
					*dp++ = '$'; *dp++ = '('; *dp++ = '(';
					while (*sp != '\0') {
						Xcheck(ds, dp);
						*dp++ = *sp++;
					}
					*dp++ = ')'; *dp++ = ')';
				} else {
					struct tbl v;
					char *p;

					v.flag = DEFINED|ISSET|INTEGER;
					v.type = 0;
					v.name[0] = '\0';
					v.val.i = evaluate(substitute(sp, 0));
					sp = strchr(sp, 0) + 1;
					for (p = strval(&v); *p; ) {
						Xcheck(ds, dp);
						*dp++ = *p++;
					}
				}
				continue;
			  case OSUBST: /* ${var{:}[=+-?]word} */
			  {
				char *varname;

				varname = sp; 		/* variable */
				sp = strchr(sp, 0) + 1;	/* skip variable */
				c = (*sp == CSUBST) ? 0 : *sp++;
				oquote = quote;
				if ((c&0x7F) == '#' || (c&0x7F) == '%') {
					x.var = global(varname);
					if (Flag(FNOUNSET) &&
					    strval(x.var) == null)
						errorf("%s: unset variable\n",
							varname);
					trimming++;
					type = XBASE;
					quote = 0;
				} else
					type = varsub(&x, varname, c);
				if (f&DOBLANK)
					doblank++;
				if (type == XBASE) {	/* expand? */
					if (st == subtype)
						errorf("ridiculous ${} nesting\n");
					--st;
					st->type = c;
					st->base = Xsavepos(ds, dp);
					st->var = x.var;
					st->quote = oquote;
				} else
					/* skip word */
					sp = (char *) wdscan(sp, CSUBST);
				continue;
			  }
			  case CSUBST: /* only get here if expanding word */
				*dp = 0;
				if (f&DOGLOB)
					f &= ~DOPAT;
				if (f&DOBLANK)
					doblank--;
				switch (st->type&0x7F) {
				  case '#':
				  case '%':
					*dp = 0;
					dp = Xrestpos(ds, dp, st->base);
					quote = st->quote;
					/* Must use st->var since calling
					 * global would cause with things
					 * like x[i+=1].
					 */
					x.str = trimsub(strval(st->var),
						dp, st->type);
					type = XSUB;
					if (f&DOBLANK)
						doblank++;
					trimming--;
					st++;
					continue;
				  case '=':
					/* not very informative.. */
					if (!st->var)
						errorf("bad substitution");
					if (st->var->flag & RDONLY)
						errorf("cannot set readonly %s\n", st->var->name);
					/* Must use st->var since calling
					 * global would cause with things
					 * like x[i+=1].
					 */
					setstr(st->var,
					       Xrestpos(ds, dp, st->base));
					break;
				  case '?':
					if (dp == Xrestpos(ds, dp, st->base))
						errorf("missing value for %s\n", st->var->name);
					else
						errorf("%s\n", Xrestpos(ds, dp, st->base));
				}
				st++;
				type = XBASE;
				continue;
			}
			break;

		  case XNULLSUB:
			/* Special case for "$@" (and "${foo[@]}") - no
			 * word is generated if $# is 0 (unless there is
			 * other stuff inside the quotes).
			 */
			type = XBASE;
			if (f&DOBLANK) {
				doblank--;
				/* not really correct: x=; "$x$@" should
				 * generate a null argument and
				 * set A; "${@:+}" shouldn't.
				 */
				if (dp == Xstring(ds, dp))
					word = IFS_WS;
			}
			continue;

		  case XSUB:
			if ((c = *x.str++) == 0) {
				type = XBASE;
				if (f&DOBLANK)
					doblank--;
				continue;
			}
			break;

		  case XARGSEP:
			type = XARG;
			quote = 1;
		  case XARG:
			if ((c = *x.str++) == 0) {
				/* force null words to be created so
				 * set -- '' 2 ''; foo "$@" will do
				 * the right thing
				 */
				if (quote && x.split)
					word = IFS_WORD;
				if ((x.str = *x.u.strv++) == NULL) {
					type = XBASE;
					if (f&DOBLANK)
						doblank--;
					continue;
				}
				c = ifs0;
				if (c == 0) {
					if (quote && !x.split)
						continue;
					c = ' ';
				}
				if (quote && x.split) {
					/* terminate word for "$@" */
					type = XARGSEP;
					quote = 0;
				}
			}
			break;

		  case XCOM:
			if (newlines) {		/* Spit out saved nl's */
				c = '\n';
				--newlines;
			} else {
				while ((c = shf_getc(x.u.shf)) == 0 || c == '\n')
				    if (c == '\n')
					    newlines++;	/* Save newlines */
				if (newlines && c != EOF) {
					shf_ungetc(c, x.u.shf);
					c = '\n';
					--newlines;
				}
			}
			if (c == EOF) {
				newlines = 0;
				shf_close(x.u.shf);
				if (x.split)
					waitlast();
				type = XBASE;
				if (f&DOBLANK)
					doblank--;
				continue;
			}
			break;
		}

		/* check for end of word or IFS separation */
		if (c == 0 || (!quote && doblank && ctype(c, C_IFS))) {
			/* How words are broken up:
			 *		   |       value of c
			 *	  word	   |	ws	nws	0
			 *	-----------------------------------
			 *	IFS_WORD	w/WS	w/NWS	w
			 *	IFS_WS		-/WS	-/NWS	-
			 *	IFS_NWS		-/NWS	w/NWS	w
			 *   (w means generate a word)
			 * Note that IFS_NWS/0 generates a word (at&t ksh
			 * doesn't do this, but POSIX does).
			 */
			if (word == IFS_WORD
			    || (word == IFS_NWS && !ctype(c, C_IFSWS)))
			{
				char *p;

				*dp++ = 0;
				p = Xclose(ds, dp);
				if (fdo&DOGLOB)
					glob(p, wp);
				else
					XPput(*wp, p);
				fdo = 0;
				if (c != 0)
					Xinit(ds, dp, 128);
			}
			if (c == 0)
				return;
			if (word != IFS_NWS)
				word = ctype(c, C_IFSWS) ? IFS_WS : IFS_NWS;
		} else {
			/* mark any special second pass chars */
			if (!quote)
				switch (c) {
				  case '*':
				  case '?':
				  case '[':
					if (f&(DOPAT|DOGLOB) || trimming) {
						fdo |= (f&DOGLOB);
						*dp++ = MAGIC;
					}
					break;
				  case NOT:
					if ((f&(DOPAT|DOGLOB) || trimming) &&
					    dp[-1] == '[' && dp[-2] == MAGIC) {
						*dp++ = MAGIC;
					}
					break;
				  case '=':
					/* note first = field tilde expansion */
					if (type == XBASE && !firsteq)
						firsteq = sp;
					break;
				  case '~':
					if (type == XBASE
					    && (((f&(DOTILDE|DOASNTILDE))
						  && sp == cp + 2) /* start */
						|| ((f&DOASNTILDE)
						    && firsteq
						    && sp >= cp + 4
						    && ((sp[-3] == ':'
							 && sp[-4] == CHAR)
							|| sp - 2 == firsteq))
						 ))
					{
						XString ts;
						char *tp, *p = sp, *r;
						char colon;

						/* : only for DOASNTILDE form */
						colon = (f&DOASNTILDE) ? ':'
						          : '/';
						Xinit(ts, tp, 10);
						while (p[0] == CHAR
						       && p[1] != '/'
						       && p[1] != colon)
						{
							Xcheck(ts, tp);
							*tp++ = p[1];
							p += 2;
						}
						*tp = '\0';
						r = (p[0] == EOS
						     || p[0] == CHAR) ?
							  tilde(Xstring(ts, tp))
							: (char *) 0;
						Xfree(ts, tp);
						if (r) {
							sp = p;
							while (*r) {
								Xcheck(ds, dp);
								*dp++ = *r++;
								word = IFS_WORD;
							}
							continue;
						}
					}
					break;
				}
			else
				quote &= ~2; /* undo temporary */

			word = IFS_WORD;
			*dp++ = c; /* save output char */
		}
	}
}

/*
 * Prepare to generate the string returned by ${} substitution.
 */
static int
varsub(xp, sp, stype)
	register Expand *xp;
	register char *sp;
	int stype;
{
	register int c;
	int type;
	char *p;
	struct tbl *vp;

	xp->var = (struct tbl *) 0;
	/* ${#var}, string length or argc */
	if (sp[0] == '#' && (c = sp[1]) != '\0') {
		int zero_ok = 0;
		/* Check for size of array */
		if ((p=strchr(sp,'[')) && (p[1]=='*'||p[1]=='@') && p[2]==']') {
			c = 0;
			vp = global(basename(sp+1));
			if (vp->flag & (ISSET|ARRAY))
				zero_ok = 1;
			for (; vp; vp = vp->array)
				if (vp->flag&ISSET)
					c = vp->index+1;
		} else if (c == '*' || c == '@')
			c = e->loc->argc;
		else {
			p = strval(global(sp+1));
			zero_ok = p != null;
			c = strlen(p);
		}
		if (Flag(FNOUNSET) && c == 0 && !zero_ok)
			errorf("%s: unset variable\n", sp + 1);
		xp->str = strsave(ulton((unsigned long)c, 10), ATEMP);
		return XSUB;
	}

	c = sp[0];
	if (c == '*' || c == '@') {
		if (e->loc->argc == 0) {
			xp->str = null;
			type = c == '@' ? XNULLSUB : XSUB;
		} else {
			xp->u.strv = e->loc->argv + 1;
			xp->str = *xp->u.strv++;
			xp->split = c == '@'; /* $@ */
			type = XARG;
		}
	} else {
		if ((p=strchr(sp,'[')) && (p[1]=='*'||p[1]=='@') && p[2]==']') {
			XPtrV wv;

			XPinit(wv, 32);
			vp = global(basename(sp));
			for (; vp; vp = vp->array) {
				if (!(vp->flag&ISSET))
					continue;
				XPput(wv, strval(vp));
			}
			if (XPsize(wv) == 0) {
				xp->str = null;
				type = p[1] == '@' ? XNULLSUB : XSUB;
				XPfree(wv);
			} else {
				XPput(wv, 0);
				xp->u.strv = (char **) XPptrv(wv);
				xp->str = *xp->u.strv++;
				xp->split = p[1] == '@'; /* ${foo[@]} */
				type = XARG;
			}
		} else {
			xp->var = global(sp);
			if ((xp->str = strval(xp->var)) == NULL)
				xp->str = null;
			type = XSUB;
		}
	}

	c = stype&0x7F;
	/* test the compiler's code generator */
	if (c == '%' || c == '#' ||
	    (((stype&0x80) ? *xp->str=='\0' : xp->str==null) ? /* undef? */
	     c == '=' || c == '-' || c == '?' : c == '+'))
		type = XBASE;	/* expand word instead of variable value */
	if (type != XBASE && Flag(FNOUNSET) && xp->str == null && c != '+')
		errorf("%s: unset variable\n", sp);
	return type;
}

/*
 * Run the command in $(...) and read its output.
 */
static int
comsub(xp, cp)
	register Expand *xp;
	char *cp;
{
	Source *s;
	register struct op *t;
	struct shf *shf;

	s = pushs(SSTRING);
	s->str = cp;
	t = compile(s);

	if (t == NULL)
		return XBASE;

	if (t != NULL && t->type == TCOM && /* $(<file) */
	    *t->args == NULL && *t->vars == NULL && t->ioact != NULL) {
		register struct ioword *io = *t->ioact;
		char *name;

		if ((io->flag&IOTYPE) != IOREAD)
			errorf("funny $() command\n");
		shf = shf_open(name = evalstr(io->name, DOTILDE), O_RDONLY, 0,
			SHF_MAPHI|SHF_CLEXEC);
		if (shf == NULL)
			errorf("%s: cannot open $() input\n", name);
		xp->split = 0;	/* no waitlast() */
	} else {
		int ofd1, pv[2];
		openpipe(pv);
		shf = shf_fdopen(pv[0], SHF_RD, (struct shf *) 0);
		ofd1 = savefd(1);	/* fd 1 may be closed... */
		dup2(pv[1], 1);
		close(pv[1]);
		execute(t, XFORK|XXCOM|XPIPEO);
		restfd(1, ofd1);
		startlast();
		xp->split = 1;	/* waitlast() */
	}

	xp->u.shf = shf;
	return XCOM;
}

/*
 * perform #pattern and %pattern substitution in ${}
 */

static char *
trimsub(str, pat, how)
	register char *str;
	char *pat;
	int how;
{
	register char *end = strchr(str, 0);
	register char *p, c, *match;

	switch (how&0xff) {	/* UCHAR_MAX maybe? */
	  case '#':		/* shortest at begin */
		for (p = str; p <= end; p++) {
			c = *p; *p = '\0';
			if (gmatch(str, pat)) {
				*p = c;
				return p;
			}
			*p = c;
		}
		break;
	case '#'|0x80:		/* longest match at begin */
		for (p = end; p >= str; p--) {
			c = *p; *p = '\0';
			if (gmatch(str, pat)) {
				*p = c;
				return p;
			}
			*p = c;
		}
		break;
	  case '%':		/* shortest match at end */
		for (p = end; p >= str; p--) {
			if (gmatch(p, pat)) {
				c = *p; *p = '\0';
				match = strsave( str, ATEMP );
				*p = c;
				return match;
			}
		}
		break;
	  case '%'|0x80:	/* longest match at end */
		for (p = str; p <= end; p++) {
			if (gmatch(p, pat)) {
				c = *p; *p = '\0';
				match = strsave( str, ATEMP );
				*p = c;
				return match;
			}
		}
		break;
	}

	return str;		/* no match, return string */
}

#ifdef BRACEEXPAND
/*	(pc@hillside.co.uk)
 *	I have decided to `fudge' alternations by picking up
 *	the compiled command tree and working with it recursively
 *	to generate the set of arguments
 *	This has the advantage of making a single discrete change
 *	to the code
 *
 *	This routine calls itself recursively
 *	a)	scan forward looking for { building the output string
 *		if none found then call expand - and exit
 *	b)	When { found, scan forward finding the end }
 *	c)	add first alternate to output string
 *	d)	scan for the end of the string copying into output
 *	e)	call routine with new string
 *	Major complication is quoting
 */
static void
alt_expand(cp, wp, f)
	char *cp;		/* input word */
	register XPtrV *wp;	/* output words */
	int f;			/* DO* flags */
{
	char *srcp = cp;
	char *left;		/* destination string of left hand side */
	char *leftend;		/* end of left hand side */
	char *alt;		/* start of alterate section */
	char *altend;		/* end of alternate section */
	char *ap;		/* working pointer */
	char *right;		/* right hand side */
	char *rp;		/* used to copy right-hand side */
	int  maxlen;		/* max string length */

	leftend = left = alloc((maxlen = alt_count(cp)), ATEMP);
	
	if (alt_scan(&srcp, &leftend, '{', 0) == 0) {
		expand(cp, wp, f&~DOGLOB);
		afree(left, ATEMP);
		return;
	}

	/*
	 *	we have a alternation section
	 */
	alt = altend = alloc(maxlen, ATEMP);

	srcp += 2;
	if (alt_scan(&srcp, &altend, '}', 1) == 0) {
		afree(left, ATEMP);
		afree(alt, ATEMP);
		/*{*/
		errorf("Missing }.\n");
	}
	*altend++ = CHAR;
	*altend++ = ',';
	*altend = EOS;
	/*
	 *	finally we may have a right-hand side
	 */
	right = srcp + 2;

	/*
	 *	glue the bits together making a new string
	 */
	for (srcp = alt; *srcp != EOS;) {

		ap = leftend;

		if (alt_scan(&srcp, &ap, ',', -1) == 0) {
			afree(left, ATEMP);
			afree(alt, ATEMP);
			errorf("Missing comma.\n");
		}
		
		srcp += 2;

		rp = right;
		(void) alt_scan(&rp, &ap, EOS, 0);

		alt_expand(left, wp, f);
	}
	afree(left, ATEMP);
	afree(alt, ATEMP);
	return;
}

/*
 * see how much space we need to hold this tree
 */
static int
alt_count(cp)
	register char *cp;
{
	register int sum = 0;
	register char *sp;

	while (*cp != EOS) {
		switch(*cp) {
		  case CHAR:
		  case QCHAR:
			sum += 2;
			cp += 2;
			break;
		  case OQUOTE:
		  case CQUOTE:
		  case CSUBST:
			sum++;
			cp++;
			break;
		  case COMSUB:
		  case EXPRSUB:
		  case OSUBST:
			sp = cp;
			cp = strchr(sp, 0) + 1;
			sum += cp - sp;
			break;
		}
	}
	return ++sum;
}

static int
alt_scan(cpp, dpp, endc, bal)
	char **cpp;		/* address of source pointer */
	char **dpp;		/* address of destination pointer */
	int endc;		/* last character we are looking for */
	int bal;
{
	register char *cp, *dp;
	int quote = 0;
	int balance = 0;
	int usebalance = 0;

	if (bal)
	{	usebalance = 1;
		balance = (bal < 1) ? 0 : 1;
	}

	cp = *cpp;
	dp = *dpp;

	while (*cp != EOS) {
		switch (*cp) {
		  case CHAR:
			if (quote == 1) {
				if (cp[1] == ']')
					quote = 0;
			}
			else
			if (quote == 0) {
				if (cp[1] == '[')
					quote = 1;
				else {
					if (usebalance) {
						if (cp[1] == '{')
							balance++;
						if (cp[1] == '}')
							balance--;
						}
					if (cp[1] == endc && balance == 0) {
						*dp = EOS;
						*dpp = dp;
						*cpp = cp;
						return 1;
						}
				}
			}
		  case QCHAR:
			*dp++ = *cp++;
		  case CSUBST:
		  copy:
			*dp++ = *cp++;
			break;
			
		  case OQUOTE:
			quote = 1;
			goto copy;

		  case CQUOTE:
			quote = 0;
			goto copy;

		  case COMSUB:
		  case EXPRSUB:
		  case OSUBST:
			while ((*dp++ = *cp++))
				;
			break;
		}
	}
	*dp = EOS;
	*cpp = cp;
	*dpp = dp;
	return 0;
}
#endif	/* BRACEEXPAND */

/*
 * glob
 * Name derived from V6's /etc/glob, the program that expanded filenames.
 */

static	char   *debunk();

static void
glob(cp, wp)
	char *cp;
	register XPtrV *wp;
{
	char path [PATH];
	register char *sp = cp;
	int oldsize;

	oldsize = XPsize(*wp);
	/* todo: check path doesn't get overrun */
	globit(path, path, sp, wp, 0);

	if (XPsize(*wp) == oldsize)
		XPput(*wp, debunk(cp));
	else
		qsortp(XPptrv(*wp) + oldsize, (size_t)(XPsize(*wp) - oldsize), xstrcmp);
}

static void
globit(ds, dp, sp, wp, check)
	char *ds;		/* dest path */
	char *dp;		/* dest end */
	char *sp;		/* source path */
	register XPtrV *wp;	/* output list */
	int check;		/* check dest existence */
{
	register char *np;	/* next source component */
	register char *tsp, *tdp;

	if (sp == NULL) {	/* end of source path */
		if (check && eaccess(ds, F_OK) < 0)
			return;
		XPput(*wp, strsave(ds, ATEMP));
		return;
	}

	if (dp > ds)
		*dp++ = '/';
	while (*sp == '/')
		*dp++ = *sp++;
	np = strchr(sp, '/');
	if (np != NULL)
		*np++ = 0;

	*dp = 0;
	if (strchr(sp, MAGIC) == NULL) { /* contains no pattern? */
		tdp = dp; tsp = sp;
		while ((*tdp++ = *tsp++) != 0)
			;
		--tdp;
		globit(ds, tdp, np, wp, check);
	} else {
		DIR *dirp;
		struct dirent *d;

		dirp = ksh_opendir((*ds == 0) ? "." : ds);
		if (dirp == NULL)
			goto Nodir;
		while ((d = readdir(dirp)) != NULL) {
			tsp = d->d_name;
			if (tsp[0] == '.' &&
			    (tsp[1] == 0 || (tsp[1] == '.' && tsp[2] == 0)))
				continue; /* always ignore . and .. */
			if ((*tsp == '.' && *sp != '.') || !gmatch(tsp, sp))
				continue;

			tdp = dp;
			while ((*tdp++ = *tsp++) != 0)
				;
			--tdp;
			globit(ds, tdp, np, wp, np != NULL);
		}
		closedir(dirp);
	  Nodir:;
	}

	if (np != NULL)
		*--np = '/';
}

/* remove MAGIC from string */
static char *
debunk(cp)
	char *cp;
{
	register char *dp, *sp;

	for (dp = sp = cp; *sp != 0; sp++)
		if (*sp != MAGIC)
			*dp++ = *sp;
	*dp = 0;
	return cp;
}

/*
 * tilde expansion
 *
 * based on a version by Arnold Robbins
 */

static char *
tilde(cp)
	char *cp;
{
	char *dp;

	if (cp[0] == '\0')
		dp = strval(global("HOME"));
	else if (cp[0] == '+' && cp[1] == '\0')
		dp = strval(global("PWD"));
	else if (cp[0] == '-' && cp[1] == '\0')
		dp = strval(global("OLDPWD"));
	else
		dp = homedir(cp);
	return dp;
}

/*
 * map userid to user's home directory.
 * note that 4.3's getpw adds more than 6K to the shell,
 * and the YP version probably adds much more.
 * we might consider our own version of getpwnam() to keep the size down.
 */

static char *
homedir(name)
	char *name;
{
	register struct tbl *ap;
	register struct passwd *pw;
	extern struct passwd *getpwnam();

	ap = tenter(&homedirs, name, hash(name));
	if (!(ap->flag & ISSET)) {
		pw = getpwnam(name);
		if (pw == NULL)
			return NULL;
		ap->val.s = strsave(pw->pw_dir, APERM);
		ap->flag |= DEFINED|ISSET|ALLOC;
	}
	return ap->val.s;
}
