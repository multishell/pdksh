/*
 * End of configuration stuff for PD ksh.
 *
 * RCSid: $Id$
 */

/* No ksh features means no editing, history or brace expansion... */
#if !defined(KSH)
# undef EMACS
# undef VI
# undef COMPLEX_HISTORY
# undef EASY_HISTORY
# undef HISTORY
#else
# define HISTORY
#endif /* !KSH */

/*
 * if you don't have mmap() you can't use Peter Collinson's history
 * mechanism.  If that is the case, then define EASY_HISTORY
 */
#if defined(HISTORY) && (!defined(COMPLEX_HISTORY) || !defined(HAVE_MMAP) || !defined(HAVE_FLOCK))
# undef COMPLEX_HISTORY
# define EASY_HISTORY			/* sjg's trivial history file */
#endif

/* Can we safely catch sigchld and wait for processes? */
#if (defined(HAVE_WAITPID) || defined(HAVE_WAIT3)) \
    && (defined(POSIX_SIGNALS) || defined(BSD42_SIGNALS))
# define JOB_SIGS
#endif

#if !defined(JOB_SIGS) || !(defined(POSIX_PGRP) || defined(BSD_PGRP))
# undef JOBS /* if no JOB_SIGS, no job control support */
#endif

/* pdksh assumes system calls return EINTR if a signal happened (this so
 * the signal handler doesn't have to longjmp()).  I don't know if this
 * happens (or can be made to happen) with sigset() et. al., so, to be on
 * the safe side, make sure a bogus shell doesn't get compiled.
 * If BSD41_SIGNALS isn't defined and your compiler chokes on this, delete
 * the hash in front of the error (and file a bug report).
 */
#ifdef BSD41_SIGNALS
  # error pdksh needs interruptable system calls.
#endif /* BSD41_SIGNALS */

#ifdef HAVE_GCC_FUNC_ATTR
# define GCC_FUNC_ATTR(x)	__attribute__((x))
# define GCC_FUNC_ATTR2(x,y)	__attribute__((x,y))
#else
# define GCC_FUNC_ATTR(x)
# define GCC_FUNC_ATTR2(x,y)
#endif /* HAVE_GCC_FUNC_ATTR */

#if defined(EMACS) || defined(VI)
# define	EDIT
#else
# undef		EDIT
#endif
