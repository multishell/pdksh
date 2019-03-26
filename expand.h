/*
 * Expanding strings
 */
/* $Id$ */


#if 0				/* Usage */
	XString xs;
	char *xp;

	Xinit(xs, xp, 128);	/* allocate initial string */
	while ((c = generate()) {
		Xcheck(xs, xp);	/* expand string if neccessary */
		Xput(xs, xp, c); /* add character */
	}
	return Xclose(xs, xp);	/* resize string */
/*
 * NOTE:
 *	The Xcheck and Xinit macros have a magic + 8 in the lengths.  This is
 *	so that you can put up to 4 characters in a XString before calling
 *	Xcheck.  (See yylex in lex.c)
 */
#endif /* 0 */

typedef struct XString {
	char   *end, *beg;	/* end, begin of string */
	size_t	len;		/* length */
} XString;

typedef char * XStringP;

/* initialize expandable string */
#define	Xinit(xs, xp, length) do { \
			(xs).len = length; \
			(xs).beg = alloc((xs).len + 8, ATEMP); \
			(xs).end = (xs).beg + (xs).len; \
			xp = (xs).beg; \
		} while (0)

/* stuff char into string */
#define	Xput(xs, xp, c)	(*xp++ = (c))

/* check for overflow, expand string */
#define	Xcheck(xs, xp) do { \
		    if (xp >= (xs).end) { \
			char *old_beg = (xs).beg; \
			(xs).len += (xs).len; /* double size */ \
			(xs).beg = aresize((xs).beg, (xs).len + 8, ATEMP); \
			(xs).end = (xs).beg + (xs).len; \
			xp = (xs).beg + (xp - old_beg); /* adjust pointer */ \
		    } \
		} while (0)

/* free string */
#define	Xfree(xs, xp)	afree((void*) (xs).beg, ATEMP)

/* close, return string */
#define	Xclose(xs, xp)	(char*) aresize((void*)(xs).beg, \
					(size_t)(xp - (xs).beg), ATEMP)
/* begin of string */
#define	Xstring(xs, xp)	((xs).beg)

#define	Xlength(xs, xp) (xp - (xs).beg)
#define	Xsavepos(xs, xp) (xp - (xs).beg)
#define	Xrestpos(xs, xp, n) ((xs).beg + (n))

/*
 * expandable vector of generic pointers
 */

typedef struct XPtrV {
	void  **cur;		/* next avail pointer */
	void  **beg, **end;	/* begin, end of vector */
} XPtrV;

#define	XPinit(x, n) do { \
			register void **vp__; \
			vp__ = (void**) alloc(sizeofN(void*, n), ATEMP); \
			(x).cur = (x).beg = vp__; \
			(x).end = vp__ + n; \
		    } while (0)

#define	XPput(x, p) do { \
			if ((x).cur >= (x).end) { \
				int n = XPsize(x); \
				(x).beg = (void**) aresize((void*) (x).beg, \
						   sizeofN(void*, n*2), ATEMP); \
				(x).cur = (x).beg + n; \
				(x).end = (x).cur + n; \
			} \
			*(x).cur++ = (p); \
		} while (0)

#define	XPptrv(x)	((x).beg)
#define	XPsize(x)	((x).cur - (x).beg)

#define	XPclose(x)	(void**) aresize((void*)(x).beg, \
					 sizeofN(void*, XPsize(x)), ATEMP)

#define	XPfree(x)	afree((void*) (x).beg, ATEMP)
