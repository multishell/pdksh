#include <string.h>
/* $Id: strrchr.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/*
 * strrchr - find last occurrence of a character in a string
 */

char *				/* found char, or NULL if none */
strrchr(s, charwanted)
const char *s;
register char charwanted;
{
	register const char *scan;
	register const char *place;

	place = NULL;
	for (scan = s; *scan != '\0'; scan++)
		if (*scan == charwanted)
			place = scan;
	if (charwanted == '\0')
		return(scan);
	return(place);
}
