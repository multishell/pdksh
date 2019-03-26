#include <string.h>
/* $Id: strcpy.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/*
 * strcpy - copy string src to dst
 */
char *				/* dst */
strcpy(dst, src)
char *dst;
const char *src;
{
	register char *dscan;
	register const char *sscan;

	dscan = dst;
	sscan = src;
	while ((*dscan++ = *sscan++) != '\0')
		continue;
	return(dst);
}
