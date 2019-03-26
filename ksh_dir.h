/* Wrapper around the ugly dir includes/ifdefs */
/* $Id$ */

#if defined(DIRENT) || defined(_POSIX_VERSION)
# include <dirent.h>
#else
# define dirent direct
# ifdef SYSNDIR
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef SYSDIR
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef NDIR
#  include <ndir.h>
# endif /* NDIR */
#endif /* DIRENT || _POSIX_VERSION */

#ifdef OPENDIR_DOES_NONDIR
extern DIR *ksh_opendir ARGS((char *d));
#else /* OPENDIR_DOES_NONDIR */
# define ksh_opendir(d)	opendir(d)
#endif /* OPENDIR_DOES_NONDIR */
