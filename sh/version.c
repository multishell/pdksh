/*
 * value of $KSH_VERSION
 */

#ifndef lint
static char *RCSid = "$Id: version.c,v 1.4 1992/05/12 09:30:37 sjg Exp $";
#endif

#include "stdh.h"
#include <setjmp.h>
#include "sh.h"
#include "patchlevel.h"

char ksh_version [] =
	"KSH_VERSION=@(#)PD KSH v4.5 92/05/12";

/***
$Log: version.c,v $
 * Revision 1.4  1992/05/12  09:30:37  sjg
 * see ChangeLog
 *
 * Revision 1.3  1992/05/03  08:29:20  sjg
 * Update for Patch05
 *
 * Revision 1.2  1992/04/25  08:33:28  sjg
 * Added RCS key.
 *
 * Revision 1.1  1992/04/18  05:51:48  sjg
 * Initial revision
 *
Version  4.0  91/11/09  sjg
distribution
Revision 3.3  89/03/27  15:52:29  egisin
distribution

Revision 3.2  88/12/14  20:10:41  egisin
many fixes

Revision 3.1  88/11/03  09:18:36  egisin
alpha distribution

Revision 1.3  88/10/20  17:34:03  egisin
added @(#) to ksh_version

Revision 1.2  88/09/27  19:01:58  egisin
fix version.c

Revision 1.1  88/09/27  18:59:06  egisin
Initial revision
***/

