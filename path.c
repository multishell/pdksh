#if !defined(lint) && !defined(no_RCSids)
static char *RCSid =
"$Id: path.c,v 1.2 1994/05/19 18:32:40 michael Exp michael $";
#endif

/*
 *	Contains a routine to search a : seperated list of
 *	paths (a la CDPATH) and make appropiate file names.
 *	Also contains a routine to simplify .'s and ..'s out of
 *	a path name.
 *
 *	Larry Bouzane (larry@cs.mun.ca)
 */

/*
 * $Log: path.c,v $
 * Revision 1.2  1994/05/19  18:32:40  michael
 * Merge complete, stdio replaced, various fixes. (pre autoconf)
 *
 * Revision 1.1  1994/04/06  13:14:03  michael
 * Initial revision
 *
 * Revision 4.2  1990/12/06  18:05:24  larry
 * Updated test code to reflect parameter change.
 * Fixed problem with /a/./.dir being simplified to /a and not /a/.dir due
 * to *(cur+2) == *f test instead of the correct cur+2 == f
 *
 * Revision 4.1  90/10/29  14:42:19  larry
 * base MUN version
 * 
 * Revision 3.1.0.4  89/02/16  20:28:36  larry
 * Forgot to set *pathlist to NULL when last changed make_path().
 * 
 * Revision 3.1.0.3  89/02/13  20:29:55  larry
 * Fixed up cd so that it knew when a node from CDPATH was used and would
 * print a message only when really necessary.
 * 
 * Revision 3.1.0.2  89/02/13  17:51:22  larry
 * Merged with Eric Gisin's version.
 * 
 * Revision 3.1.0.1  89/02/13  17:50:58  larry
 * *** empty log message ***
 * 
 * Revision 3.1  89/02/13  17:49:28  larry
 * *** empty log message ***
 * 
 */

/*
 *	Makes a filename into result using the following algorithm.
 *	- make result NULL
 *	- if file starts with '/', append file to result & set pathlist to NULL
 *	- if file starts with ./ or ../ append curpath and file to result
 *	  and set pathlist to NULL
 *	- if the first element of pathlist doesnt start with a '/' xx or '.' xx
 *	  then curpath is appended to result.
 *	- the first element of pathlist is appended to result
 *	- file is appended to result
 *	- pathlist is set to the start of the next element in pathlist (or NULL
 *	  if there are no more elements.
 *	The return value indicates whether a non-null element from pathlist
 *	was appened to result.
 */
int
make_path(curpath, file, pathlist, result, len)
	char	*curpath;
	char	*file;
	char	**pathlist;	/* & of : seperated list (a la PATH/CDPATH) */
	char	*result;
	int	len;		/* size of result */
{
	int	rval = 0;
	int	use_pathlist = 1;
	char	c;
	char	*plist;

	if (file == (char *) 0)
		file = "";

	if (*file == '/') {
		while (--len > 0 && *file != '\0')
			*result++ = *file++;
		*result = '\0';
		*pathlist = (char *) 0;
		return 0;
	}

	if (*file == '.') {
		if ((c = *(file+1)) == '.') {
			if ((c = *(file+2)) == '/' || c == '\0')
				use_pathlist = 0;
		} else if (c == '/' || c == '\0')
			use_pathlist = 0;
	}

	plist = *pathlist;
	if (plist == (char *) 0)
		plist = "";

	if (use_pathlist == 0 || *plist != '/') {
		if (curpath != (char *) 0 && *curpath != '\0') {
			while (--len > 0 && *curpath != '\0')
				*result++ = *curpath++;
			if (--len > 0)
				*result++ = '/';
		}
	}
	if (use_pathlist) {
		if (*plist != '\0' && *plist != ':') {
			rval = 1;
			while (*plist != '\0' && *plist != ':') {
				if (--len > 0)
					*result++ = *plist++;
			}
			if (--len > 0)
				*result++ = '/';
			if (*plist == ':' && *(plist+1) != '\0')
				plist++;
		} else if (*plist == ':')
			plist++;
	}
	while (--len > 0 && *file != '\0')
		*result++ = *file++;

	*result = '\0';
	*pathlist = (use_pathlist && *plist != '\0') ? plist : (char *) 0;
	return rval;
}

/*
 * Simplify pathnames containing "." and ".." entries.
 * ie, path_simplify("/a/b/c/./../d/..") returns "/a/b"
 */
void
simplify_path(path)
	char	*path;
{
	char	*f;
	char	*cur = path;
	char	*t = path + 1;

	/* Only simplify absolute paths */
	if (*cur != '/')
		return;

	while (1) {
		/* treat multiple '/'s as one '/' */
		while (*t == '/')
			t++;

		if (*t == '\0') {
			if (cur != path)
				*cur = '\0';
			else
				*(cur+1) = '\0';
			break;
		}

		/* find/copy next component of pathname */
		*cur = '/';
		f = cur + 1;
		while (*t != '\0' && *t != '/')
			*f++ = *t++;

		/* check for a ". or ".." entry and simplify */
		if (*(cur+1) == '.') {
			if (*(cur+2) == '.' && (cur+3) == f) {
				if (cur != path) {
					cur -= 2;
					while (*cur != '/')
						cur--;
				}
			} else if ((cur+2) != f)
				cur = f;
		} else
			cur = f;
	}
}

#ifdef	TEST
void	simplify_path();
int	make_path();

main(argc, argv)
{
	int	rv;
	char	*cp, cdpath[256], pwd[256], file[256], result[256];

	printf("enter CDPATH: "); gets(cdpath);
	printf("enter PWD: "); gets(pwd);
	while (1) {
		if (printf("Enter file: "), gets(file) == 0)
			return 0;
		cp = cdpath;
		do {
			rv = make_path(pwd, file, &cp, result, sizeof(result));
			printf("make_path returns (%d), \"%s\" ", rv, result);
			simplify_path(result);
			printf("(simpifies to \"%s\")\n", result);
		} while (cp);
	}
}
#endif	/* TEST */
