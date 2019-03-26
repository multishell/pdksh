#include <string.h>
/* $Id: strncmp.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/*
 * strncmp - compare at most n characters of string s1 to s2
 */

int				/* <0 for <, 0 for ==, >0 for > */
strncmp(s1, s2, n)
const char *s1;
const char *s2;
size_t n;
{
	register const char *scan1;
	register const char *scan2;
	register size_t count;

	scan1 = s1;
	scan2 = s2;
	count = n;
	while (--count >= 0 && *scan1 != '\0' && *scan1 == *scan2) {
		scan1++;
		scan2++;
	}
	if (count < 0)
		return(0);

	/*
	 * The following case analysis is necessary so that characters
	 * which look negative collate low against normal characters but
	 * high against the end-of-string NUL.
	 */
	if (*scan1 == '\0' && *scan2 == '\0')
		return(0);
	else if (*scan1 == '\0')
		return(-1);
	else if (*scan2 == '\0')
		return(1);
	else
		return(*scan1 - *scan2);
}
