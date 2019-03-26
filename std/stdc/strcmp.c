#include <string.h>
/* $Id: strcmp.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/* Modified by Eric Gisin */

/*
 * strcmp - compare string s1 to s2
 */

int				/* <0 for <, 0 for ==, >0 for > */
strcmp(s1, s2)
const char *s1;
const char *s2;
{
	register const char *scan1;
	register const char *scan2;
#if 0				/* some machines prefer int to char */
	register int c1, c2;
#else
	register char c1, c2;
#endif

	scan1 = s1;
	scan2 = s2;
	while ((c1 = *scan1++) == (c2 = *scan2++) && c1 != 0)
		;

	/*
	 * The following case analysis is necessary so that characters
	 * which look negative collate low against normal characters but
	 * high against the end-of-string NUL.
	 */
	if (c1 == '\0' && c2 == '\0')
		return(0);
	else if (c1 == '\0')
		return(-1);
	else if (c2 == '\0')
		return(1);
	else
		return(c1 - c2);
}
