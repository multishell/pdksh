/* $Id: memchr.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#include <string.h>

void *
memchr(ap, c, n)
	const void *ap;
	register int c;
	register size_t n;
{
	register char *p = ap;

	if (n++ > 0)
		while (--n > 0)
			if (*p++ == c)
				return --p;
	return NULL;
}

