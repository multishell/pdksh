/*
 * Expanding strings
 */

#if 0				/* Usage */
	XString xs;
	char *xp;

	Xinit(xs, xp, 128);	/* allocate initial string */
	while ((c = generate()) {
		Xcheck(xs, xp);	/* expand string if neccessary */
		Xput(xs, xp, c); /* add character */
	}
	return Xclose(xs, xp);	/* resize string */
#endif

typedef struct XString {
	char   *end, *beg;	/* end, begin of string */
#if 1
	char   *oth, *old;	/* togo, adjust */
#endif
	size_t	len;		/* length */
} XString;

typedef char * XStringP;

/* initialize expandable string */
#define	Xinit(xs, xp, length) { \
			(xs).len = length; \
			(xs).beg = alloc((xs).len + 8, ATEMP); \
			(xs).end = (xs).beg + (xs).len; \
			xp = (xs).beg; \
		}

/* stuff char into string */
#define	Xput(xs, xp, c)	*xp++ = (c)

/* check for overflow, expand string */
#define	Xcheck(xs, xp) if (xp >= (xs).end) { \
			char *old_beg = (xs).beg; \
			(xs).len += (xs).len; /* double size */ \
			(xs).beg = aresize((xs).beg, (xs).len + 8, ATEMP); \
			(xs).end = (xs).beg + (xs).len; \
			xp = (xs).beg + (xp - old_beg); /* adjust pointer */ \
		}

/* free string */
#define	Xfree(xs, xp)	afree((void*) (xs).beg, ATEMP)

/* close, return string */
#define	Xclose(xs, xp)	(char*) aresize((void*)(xs).beg, \
					(size_t)(xp - (xs).beg), ATEMP)
/* begin of string */
#define	Xstring(xs, xp)	((xs).beg)

#define	Xsavepos(xs, xp) (xp - (xs).beg)
#define	Xrestpos(xs, xp, n) ((xs).beg + (n))

/*
 * expandable vector of generic pointers
 */

typedef struct XPtrV {
	void  **cur;		/* next avail pointer */
	void  **beg, **end;	/* begin, end of vector */
} XPtrV;

#define	XPinit(x, n) { \
			register void **vp; \
			vp = (void**) alloc(sizeofN(void*, n), ATEMP); \
			(x).cur = (x).beg = vp; \
			(x).end = vp + n; \
			}

#define	XPput(x, p) { \
			if ((x).cur >= (x).end) { \
				int n = XPsize(x); \
				(x).beg = (void**) aresize((void*) (x).beg, \
						   sizeofN(void*, n*2), ATEMP); \
				(x).cur = (x).beg + n; \
				(x).end = (x).cur + n; \
			} \
			*(x).cur++ = (p); \
			}

#define	XPptrv(x)	((x).beg)
#define	XPsize(x)	((x).cur - (x).beg)

#define	XPclose(x)	(void**) aresize((void*)(x).beg, \
					 sizeofN(void*, XPsize(x)), ATEMP)

#define	XPfree(x)	afree((void*) (x).beg, ATEMP)

