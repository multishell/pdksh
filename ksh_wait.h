/* Wrapper around the ugly sys/wait includes/ifdefs */
/* $Id$ */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_UNION_WAIT
typedef union wait WAIT_T;
# ifndef WIFCORED
#  define WIFCORED(s)		((s).w_coredump)
# endif
# define WSTATUS(s)		((s).w_status)

# ifndef WEXITSTATUS
#  define WEXITSTATUS(s)	((s).w_retcode)
# endif
# ifndef WTERMSIG
#  define WTERMSIG(s)		((s).w_termsig)
# endif
# ifndef WSTOPSIG
#  define WSTOPSIG(s)		((s).w_stopsig)
# endif
#else /* HAVE_UNION_WAIT */
typedef int WAIT_T;
# ifndef WIFCORED
#  define WIFCORED(s)		((s) & 0x80)
# endif
# define WSTATUS(s)		(s)

# ifndef WIFEXITED
#  define WIFEXITED(s)		(((s) & 0xff) == 0)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(s)	(((s) >> 8) & 0xff)
# endif
# ifndef WIFSIGNALED
#  define WIFSIGNALED(s)	(((s) & 0xff) != 0 && ((s) & 0xff) != 0x7f)
# endif
# ifndef WTERMSIG
#  define WTERMSIG(s)		((s) & 0x7f)
# endif
# ifndef WIFSTOPPED
#  define WIFSTOPPED(s)		(((s) & 0xff) == 0x7f)
# endif
# ifndef WSTOPSIG
#  define WSTOPSIG(s)		(((s) >> 8) & 0xff)
# endif
#endif /* HAVE_UNION_WAIT */
