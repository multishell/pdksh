/* NAME:
 *      trace.c - a simple trace facility
 *
 * SYNOPSIS:
 *      TRACE(level, (fmt [, ...]));
 *
 * DESCRIPTION:
 *      This module provides a simple trace facility via
 *      a call to checkpoint() which opens a log file and writes 
 *      an entry and closes the log (so that process crashes 
 *      won't destroy the log :-).  checkpoint() takes options 
 *      like printf().  If Trace_log is not initialized then
 *      stderr is used.
 *      
 *      The header file trace.h defines a macro TRACE() which 
 *      is useful in that it allows checkpoint to be called 
 *      based on the value of Trace_level, and the calls can be 
 *      eliminated by undefining USE_TRACE.
 *
 * RETURN VALUE:
 *      None.
 *
 * FILES:
 *      None.
 *
 * SEE ALSO:
 *      
 *
 * BUGS:
 *      
 *
 * AMENDED:
 *      91/11/22  22:53:56  (sjg)
 *
 * RELEASED:
 *      91/11/22  22:54:17  v1.2
 *
 *      @(#)Copyright (c) 1990 Simon J. Gerraty.
 */
#ifdef USE_TRACE

#ifndef lint
static char  sccs_id[] = "@(#)trace.c     1.2  91/11/22  22:53:56  (sjg)";
#endif

/* include files */
#include <stdio.h>
#ifdef __STDC__
# include <stdlib.h>
#endif

#define EXTERN
#include "trace.h"
#undef EXTERN

/* some useful #defines */
#ifndef ENTRY
# define ENTRY
# define LOCAL static
# define BOOLEAN int
#endif
#ifndef TRUE
# define TRUE  1
# define FALSE 0
#endif
#ifndef ARGS
# if defined(__STDC__) || defined(PROTO)
#   define ARGS(p) p
# else
#   define ARGS(p) ()
# endif
#endif


/* NAME:
 *      checkpoint - write a logfile entry
 *
 * SYNOPSIS:
 *      checkpoint(fmt, ...)
 *
 * DESCRIPTION:
 *      This function takes a variable number or args  
 *      like the printf(3S) family of functions.
 *
 * RETURN VALUE:
 *      None
 */
extern char * _CDECL strdup	ARGS((char *s));
  
#ifdef __STDC__
# include <stdarg.h>

ENTRY void _CDECL
checkpoint(fmt)
	char *fmt;
{
  int c;
  va_list arg_ptr;
  FILE *fp;
  register char *rcp;
  char  *mode;
  static  setup;
  
  va_start(arg_ptr, fmt);
#else  /* __STDC__ */
# include <varargs.h>
  
ENTRY void _CDECL
checkpoint(va_alist)
  va_dcl
{
  extern char *getenv	ARGS((char *var));
  char *fmt;
  int c;
  va_list arg_ptr;
  FILE *fp;
  register char *rcp;
  char  *mode;
  static  setup;
  
  va_start(arg_ptr);
  fmt = va_arg(arg_ptr, char *);
#endif /* __STDC__ */
  
  /* 42 is a "magic" number */
  if (setup == 42)
    mode = "a";
  else
  {
    if (Trace_level == 0 && (rcp = getenv("TRACE_LEVEL")))
      Trace_level = atoi(rcp);
    if (Trace_log == NULL || *Trace_log == '\0')
    {
      if (rcp = getenv("TRACE_LOG"))
	Trace_log = strdup(rcp);
      else
	Trace_log = NULL;
    }
    setup = 42;
    mode= "w";
  }
  if (Trace_log)
    fp = fopen(Trace_log, mode);
  else
    fp = stderr;
  if (fp != (FILE *)NULL)
  {
    vfprintf(fp, fmt, arg_ptr);
    fputc('\n', fp);
    if (fp == stderr)
      fflush(fp);
    else
      fclose(fp);
  }
}

#endif /* USE_TRACE */

/* This lot (for GNU-Emacs) goes at the end of the file. */
/* 
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
