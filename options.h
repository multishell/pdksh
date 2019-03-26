/*
 * Options configuration file for the PD ksh
 *
 * RCSid: $Id$
 */

/* Define this to the path to use if the PATH environment variable is
 * not set (ie, either never set or explicitly unset with the unset
 * command).  A value without . in it is safest.
 * This define is NOT USED if confstr() and _CS_PATH are available or
 * if <paths.h> defines _PATH_DEFPATH.
 */
#define DEFAULT_PATH	"/bin:/usr/bin:/usr/ucb"

/* Define EMACS if you want emacs command line editing compiled in (enabled
 * with "set -o emacs", or by setting the VISUAL or EDITOR variables to
 * something ending in emacs).
 */
#define	EMACS

/* Define VI if you want vi command line editing compiled in (enabled with
 * "set -o vi", or by setting the VISUAL or EDITOR variables to something
 * ending in vi).
 */
#define	VI

/* Define JOBS if you want job control compiled in.  This requires that your
 * system support process groups and reliable signal handling routines (it
 * will be automatically undefined if your system doesn't have them).
 */
#define	JOBS

/* Define BRACEEXPAND if you want csh-like {} globbing compiled in
 * (enabled with "set -o braceexpand").
 */
#define BRACEEXPAND

/* Define COMPLEX_HISTORY if you want at&t ksh style history files (ie, file
 * is updated after each command is read; concurrent ksh's read each other's
 * commands, etc.).  This option uses the mmap() function - if mmap isn't
 * available, the option is automatically undefined.  If option is not defined,
 * a simplier history mechanism which reads/saves the history at startup/exit
 * time, respectively, is used.  COMPLEX_HISTORY is courtesy of Peter Collinson.
 */
#undef COMPLEX_HISTORY

/* Define POSIXLY_CORRECT if you want POSIX behavior by default (otherwise,
 * posix behavior is only turned on if the environment variable POSIXLY_CORRECT
 * is present or by using "set -o posix"; it can be turned off with
 * "set +o posix").
 * See the file POSIX for details on what this option affects.
 * NOTE: posix mode is not compatable with some bourne sh/at&t ksh scripts.
 */
#undef POSIXLY_CORRECT

/* Define SWTCH to handle SWITCH character, for use with shell layers (shl(1)).
 * This has not been tested for some time.
 */
#undef	SWTCH

/* SILLY: The name says it all - compile game of life code into the emacs
 * command line editing code.
 */
#undef	SILLY
