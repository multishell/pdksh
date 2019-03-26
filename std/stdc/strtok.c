#include <string.h>
/* $Id: strtok.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/*
 * Get next token from string s (NULL on 2nd, 3rd, etc. calls),
 * where tokens are nonempty strings separated by runs of
 * chars from delim.  Writes NULs into s to end tokens.  delim need not
 * remain constant from call to call.
 */

static char *scanpoint = NULL;

char *				/* NULL if no token left */
strtok(s, delim)
char *s;
register const char *delim;
{
	register char *scan;
	char *tok;
	register const char *dscan;

	if (s == NULL && scanpoint == NULL)
		return(NULL);
	if (s != NULL)
		scan = s;
	else
		scan = scanpoint;

	/*
	 * Scan leading delimiters.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0'; dscan++)
			if (*scan == *dscan)
				break;
		if (*dscan == '\0')
			break;
	}
	if (*scan == '\0') {
		scanpoint = NULL;
		return(NULL);
	}

	tok = scan;

	/*
	 * Scan token.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0';)	/* ++ moved down. */
			if (*scan == *dscan++) {
				scanpoint = scan+1;
				*scan = '\0';
				return(tok);
			}
	}

	/*
	 * Reached end of string.
	 */
	scanpoint = NULL;
	return(tok);
}
