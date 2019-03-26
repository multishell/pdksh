#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSSESMGR
#define INCL_WINPROGRAMLIST
#include <os2.h>
#include "config.h"
#include <sys/emx.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

void noinherit(int fd)
{
  DosSetFHState(fd, OPEN_FLAGS_NOINHERIT);
}

static int isfullscreen(void)
{
  PTIB ptib;
  PPIB ppib;

  DosGetInfoBlocks(&ptib, &ppib);
  return (ppib -> pib_ultype != SSF_TYPE_WINDOWABLEVIO);
}

static int 
newsession(int type, int mode, char *cmd, char **args, char **env)
{
  STARTDATA sd;
  STATUSDATA st;
  REQUESTDATA qr;
  ULONG sid, pid, len, cnt, rc;
  PVOID ptr;
  BYTE prio;
  static char queue[18];
  static HQUEUE qid = -1;
  char *ap, *ep, *p;


  for ( cnt = 1, len = 0; args[cnt] != NULL; cnt++ )
    len += strlen(args[cnt]) + 1;
  p = ap = alloca(len + 2);
  *p = 0;
  for ( cnt = 1, len = 0; args[cnt] != NULL; cnt++ )
  {
    if ( cnt > 1 )
      *p++ = ' ';
    strcpy(p, args[cnt]);
    p += strlen(p);
  }
  for ( cnt = 0, len = 0; env[cnt] != NULL; cnt++ )
    len += strlen(env[cnt]) + 1;
  p = ep = alloca(len + 2);
  *p = 0;
  for ( cnt = 0, len = 0; env[cnt] != NULL; cnt++ )
  {
    strcpy(p, env[cnt]);
    p += strlen(p) + 1;
  }
  *p = 0;

  if ( mode == P_WAIT && qid == -1 )
  {
    sprintf(queue, "\\queues\\ksh%04d", getpid());
    if ( DosCreateQueue(&qid, QUE_FIFO, queue) )
      return -1;
  }

  sd.Length = sizeof(sd);
  sd.Related = (mode == P_WAIT) ? SSF_RELATED_CHILD : SSF_RELATED_INDEPENDENT;
  sd.FgBg = SSF_FGBG_FORE;
  sd.TraceOpt = SSF_TRACEOPT_NONE;
  sd.PgmTitle = NULL;
  sd.PgmName = cmd;
  sd.PgmInputs = (PBYTE) ap;
  sd.TermQ = (mode == P_WAIT) ? (PBYTE) queue : NULL;
  sd.Environment = NULL;
  sd.InheritOpt = SSF_INHERTOPT_PARENT;
  sd.SessionType = type;
  sd.IconFile = NULL;
  sd.PgmHandle = 0;
  sd.PgmControl = 0;

  if ( DosStartSession(&sd, &sid, &pid) )
    return errno = ENOEXEC, -1;

  if ( mode == P_WAIT )
  {
    st.Length = sizeof(st);
    st.SelectInd = SET_SESSION_UNCHANGED;
    st.BondInd = SET_SESSION_BOND;
    DosSetSession(sid, &st);
    if ( DosReadQueue(qid, &qr, &len, &ptr, 0, DCWW_WAIT, &prio, 0) )
      return -1;
    rc = ((PUSHORT)ptr)[1];
    DosFreeMem(ptr);
    exit(rc);
  }
  else
    exit(0);
}

int ksh_execve(char *cmd, char **args, char **env)
{
  ULONG apptype;
  char path[256], *p;
  int rc;

  strcpy(path, cmd);
  for ( p = path; *p; p++ )
    if ( *p == '/' )
      *p = '\\';

  if ( DosQueryAppType(path, &apptype) == 0 )
  {
    if (apptype & FAPPTYP_DOS)
      return newsession(isfullscreen() ? SSF_TYPE_VDM :
                                         SSF_TYPE_WINDOWEDVDM, 
			P_NOWAIT, path, args, env);

    if ((apptype & FAPPTYP_WINDOWSREAL) ||
        (apptype & FAPPTYP_WINDOWSPROT) ||
        (apptype & FAPPTYP_WINDOWSPROT31))
      return newsession(isfullscreen() ? PROG_WINDOW_AUTO :
                                         PROG_SEAMLESSCOMMON,
			P_NOWAIT, path, args, env);

    if ( (apptype & FAPPTYP_EXETYPE) == FAPPTYP_WINDOWAPI ) {
      printf(""); /* kludge to prevent PM apps from core dumping */
      return newsession(SSF_TYPE_PM, P_NOWAIT, path, args, env);
    }

    if ( (apptype & FAPPTYP_EXETYPE) == FAPPTYP_NOTWINDOWCOMPAT ||
         (apptype & FAPPTYP_EXETYPE) == FAPPTYP_NOTSPEC )
      if ( !isfullscreen() )
        return newsession(SSF_TYPE_FULLSCREEN, P_NOWAIT, path, args, env);
  }
  if ( (rc = execve(path, args, env)) != -1 )
    exit(rc);

  return -1;
}

void UnixName(char *path)
{
  for ( ; *path; path++ )
    if ( *path == '\\' )
      *path = '/';
}

char *ksh_strchr_dirsep(const char *path)
{
  char *p1 = strchr(path, '\\');
  char *p2 = strchr(path, '/');
  if ( !p1 ) return p2;
  if ( !p2 ) return p1;
  return (p1 > p2) ? p2 : p1;
}


char *ksh_strrchr_dirsep(const char *path)
{
  char *p1 = strrchr(path, '\\');
  char *p2 = strrchr(path, '/');
  if ( !p1 ) return p2;
  if ( !p2 ) return p1;
  return (p1 > p2) ? p1 : p2;
}
