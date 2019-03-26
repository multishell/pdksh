/*
 * Reimplementation of SysVr3 sh builtin command "getopts" for S5R2 shell.
 *
 * created by Arnold Robbins
 * modified by Doug Gwyn
 * modified for PD ksh by Eric Gisin
 */

#ifndef lint
static char *RCSid = "$Id: getopts.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <setjmp.h>
#include "sh.h"

/*
 * The following is derived from getopt() source placed into the public
 * domain by AT&T (the only time they're known to have done that).
 *
 * It has been modified somewhat to fit into the context of the shell.
 *
 * -D"FASCIST" if you really want to strictly enforce ALL the
 * Command Syntax Standard rules (not recommended).
 */

#define GETOPTEOF	(-1)
#define ERR(S, C)	shellf("%s%c\n", (S), (C))

static	int	optind;
static char    *optarg;
static int	sp;

static int
getopt(argc, argv, opts)
	int argc;
	register char **argv, *opts;
{
	register int c;
	register char *cp;

	if (sp == 1)
		if (optind >= argc ||
		   argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(GETOPTEOF);
		else if (strcmp(argv[optind], "--") == 0) {
			optind++;
			return(GETOPTEOF);
		}
	c = argv[optind][sp];
	if (c == ':' || (cp=strchr(opts, c)) == NULL) {
		ERR("illegal option -- ", c);
		if (argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		optarg = NULL;
		return('?');
	}
	if (*++cp == ':') {
#ifdef FASCIST
		if (sp != 1) {
			ERR("option must not be grouped -- ", c );
			optind++;
			sp = 1;
			optarg = NULL;
			return('?');
		} else
#endif
		if (argv[optind][sp+1] != '\0') {
#ifdef FASCIST
			ERR("option must be followed by whitespace -- ", c );
			optind++;
			sp = 1;
			optarg = NULL;
			return('?');
#else
			optarg = &argv[optind++][sp+1];
#endif
		} else if (++optind >= argc) {
			ERR("option requires an argument -- ", c);
			sp = 1;
			optarg = NULL;
			return('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if (argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return(c);
}

/*
 * The following were created by Arnold Robbins.
 */

/* resetopts --- magic code for when OPTIND is reset to 1 */

void
resetopts ()
{
	optind = 1;
	sp = 1;
}

int
c_getopts(wp)
	char **wp;
{
	int ret;
	register int argc;
	char temp[2];
	char *optstr;			/* list of options */
	char *name;			/* variable to get flag val */
	char **argv;

	if ((optstr = *++wp) == NULL || (name = *++wp) == NULL)
		errorf("missing arguments\n");

	for (argc = 1; wp[argc] != NULL; argc++)
		;

	if (argc > 1)
		ret = getopt(argc, wp, optstr);
	else {
		if (**(e.loc->argv) == '\0') {
			/*
			 * When c_getopts gets called from comexec() it
			 * doesn't set up argc/argv in the local block.
			 * Maybe this should be done in newblock() but
			 * I'm not sure about the implications, and this
			 * is the only place I've been bitten so far...
			 * JRM
			 */
			argc = e.loc->next->argc;
			argv = e.loc->next->argv;
		} else {
			argc = e.loc->argc;
			argv = e.loc->argv;
		}
		ret = getopt(argc+1, argv, optstr);

	}

	/*
	 * set the OPTIND variable in any case, to handle "--" skipping
	 * unless it's 1, which would trigger a reset
	 */

	if (optind != 1)
		setint(global("OPTIND"), (long)optind);

	if (ret == GETOPTEOF)		/* end of args */
		return (1);

	/*
	 * else, got an arg, set the various shell variables
	 */

	if (optarg != NULL)
		setstr(global("OPTARG"), optarg);

	temp[0] = (char) ret;
	temp[1] = '\0';
	setstr(global(name), temp);

	return (0);
}
