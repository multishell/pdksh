/*
 * command history
 *
 * only implements in-memory history.
 */

#ifndef lint
static char *RCSid = "$Id: history.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <setjmp.h>
#include "sh.h"

char   *histrpl();
char  **current;
int	curpos;

static FILE *hist_fh = NULL;
static FILE *hist_open ARGS((char *mode));
#ifndef HISTFILE
# define HISTFILE ".pdksh_hist"
#endif

  
c_fc(wp)
	register char **wp;
{
	register char *id;
	FILE *f;
	struct temp *tf;
	register char **hp;
	char **hbeg, **hend;
	char *p, *cmd = NULL;
	int lflag = 0, nflag = 0, sflag = 0, rflag = 0, gflag = 0;
	int done = 0;
	void histbackup();

	for (wp++; (id = *wp) != NULL && *id++ == '-' && !done; wp++)
		while (*id && !done) {
			switch (*id++) {
			  case 'l':
				lflag++;
				break;
			  case 'n':
				nflag++;
				break;
			  case 'r':
				rflag++;
				break;
			  case 'g':
				gflag++;
				break;
			  case 'e':
				if (++wp && (p = *wp)) {
					if (p[0] == '-' && !p[1]) {
						sflag++;
					} else {
						cmd = alloc((size_t)(strlen(p)+4),ATEMP);
						strcpy(cmd, p);
						strcat(cmd, " $_");
					}
				} else
					errorf("argument expected\n");
				id = "";
				break;
			  default:
				wp--;
				done++;
				break;
			}
		}

	if (sflag) {
		char *pat = NULL, *rep = NULL;

		hp = histptr - 1;
		while ((id = *wp++) != NULL) {
			/* todo: multiple substitutions */
			if ((p = strchr(id, '=')) != NULL) {
				pat = id;
				rep = p;
				*rep++ = '\0';
			} else
				hp = histget(id);
		}

		if (hp == NULL || hp < history)
			errorf("cannot find history\n");
		if (pat == NULL)
			strcpy(line, *hp);
		else
			histrpl(*hp, pat, rep, gflag);
		histbackup();
		histsave(line); 
		histpush--; 
		line[0] = '\0';
		return 0;
	}

	if (*wp != NULL) {
		hbeg = histget(*wp++); /* first */
		if (*wp != NULL)
			hend = histget(*wp++); /* last */
		else if (lflag)
			hend = histptr;
		else
			hend = hbeg;
	} else {
		if (lflag)
			hbeg = histptr - 16, hend = histptr;
		else
			hbeg = hend = histptr - 1;
		if (hbeg < history)
			hbeg = history;
	}
	if (hbeg == NULL || hend == NULL)
		errorf("can't find history\n");

	if (lflag)
		f = stdout;
	else {
		nflag++;
		tf = maketemp(ATEMP);
		tf->next = e.temps; e.temps = tf;
		f = fopen(tf->name, "w");
		if (f == NULL)
			errorf("cannot create temp file %s", tf->name);
		setvbuf(f, (char *)NULL, _IOFBF, BUFSIZ);
	}

	for (hp = (rflag ? hend : hbeg); rflag ? (hp >= hbeg) : (hp <= hend);
	      rflag ? hp-- : hp++) {
		if (!nflag)
			fprintf(f, "%3d: ", source->line - (int)(histptr-hp));
		fprintf(f, "%s\n", *hp);
	}

	if (lflag)
		return 0;
	else
		fclose(f);

	setstr(local("_"), tf->name);
	if (cmd) {
		command(cmd); /* edit temp file */
		afree(cmd, ATEMP);
	} else
		command("${FCEDIT:-/bin/ed} $_");

	f = fopen(tf->name, "r");
	if (f == NULL)
		errorf("cannot open temp file %s\n", tf->name);
	setvbuf(f, (char *)NULL, _IOFBF, BUFSIZ);
	/* we push the editted lines onto the history list */
	while (fgets(line, sizeof(line), f) != NULL) {
		histsave(line); 
		histpush--; 
	}
	line[0] = '\0';
	fclose(f);

	return 0;
}

/******************************/
/* Back up over last histsave */
/******************************/
void
histbackup()
{
	static int last_line = -1;

	if (histptr > history && last_line != source->line) { 
		source->line--;
		afree((void*)*histptr, APERM);
		histptr--;
		last_line = source->line;
	}
}

/*
 * save command in history
 */
void
histsave(cmd)
	char *cmd;
{
	register char **hp = histptr;
	char *cp;

	if (++hp >= history + HISTORY) { /* remove oldest command */
		afree((void*)*history, APERM);
		for (hp = history; hp < history + HISTORY - 1; hp++)
			hp[0] = hp[1];
	}
	*hp = strsave(cmd, APERM);
	if ((cp = strchr(*hp, '\n')) != NULL)
		*cp = '\0';
	histptr = hp;
}

/*
 * get pointer to history given pattern
 * pattern is a number or string
 */
char **
histget(str)
	char *str;
{
	register char **hp = NULL;

	if (*str == '-')
		hp = histptr + getn(str);
	else
	if (digit(*str))
		hp = histptr + (getn(str) - source->line);
	else 
	if (*str == '?') {	/* unanchored match */
		for (hp = histptr-1; hp >= history; hp--)
			if (strstr(*hp, str+1) != NULL)
				break;
	} else {		/* anchored match */
		for (hp = histptr; hp >= history; hp--)
			if (strncmp(*hp, str, strlen(str)) == 0)
				break;
	}

	return (history <= hp && hp <= histptr) ? hp : NULL;
}

char *
histrpl(s, pat, rep, global)
	char *s;
	char *pat, *rep;
	int global;
{
	char *s1, *p, *last = NULL;
	int len = strlen(pat);

	if (strlen(s) - strlen(pat) + strlen(rep) >= LINE)
		errorf("substitution too long\n");
	line[0] = '\0';
	p = line;
	while (s1 = strstr(s, pat)) {
		strncpy(p, s, s1 - s);		/* first part */
		strcpy(p + (s1 - s), rep);	/* replacement */
		s = s1 + len;
		last = s1;
		p = strchr(p, 0);
		if (!global)
			s = "";
	}
	if (last)
		strcpy(p, last + len);		/* last part */
	else
		errorf("substitution failed\n");
	return line;
}

/*
 * 92-04-25 <sjg@zen>
 * A simple history file implementation.
 * At present we only save the history when we exit.
 * This can cause problems when there are multiple shells are 
 * running under the same user-id.  The last shell to exit gets 
 * to save its history.
 */
void
hist_init(s)
  Source *s;
{
  static int once = 0;
  FILE *fh;
  
  if (once++)
    return;

  if (fh = hist_open("r"))
  {
    while (fgets(line, sizeof(line), fh) != NULL)
    {
      histsave(line); 
      s->line++;
    }
    line[0] = '\0';
    fclose(fh);
#if 0	/* this might be a good idea? */
    hist_fh = hist_open("a");
#endif
  }
  
}


/*
 * save our history.
 * We check that we do not have more than we are allowed.
 * If the history file is read-only we do nothing.
 * Handy for having all shells start with a useful history set.
 */

void
hist_finish()
{
  static int once = 0;
  FILE *fh;
  register int i, mx;
  register char **hp, *mode = "w";
  
  if (once++)
    return;
  if ((mx = atoi(strval(global("HISTSIZE")))) > HISTORY || mx <= 0)
    mx = HISTORY;
  /* check how many we have */
  i = histptr - history;
  if (i >= mx)
  {
    hp = &histptr[-mx];
  }
  else
  {
    hp = history;
  }
  if (fh = hist_open(mode))
  {
    for (i = 0; i < mx && hp[i]; i++)
      fprintf(fh, "%s\n", hp[i]);
    fclose(fh);
  }
}


/*
 * simply grab the nominated history file.
 */
static FILE *
hist_open(mode)
  char *mode;
{
  register char *rcp;
  FILE *fh;
  char name[128];
  
  if ((rcp = strval(global("HISTFILE"))) == NULL || *rcp == '\0')
  {
    (void) sprintf(name, "%s/%s", strval(global("HOME")), HISTFILE);
    rcp = name;
  }
  return fopen(rcp, mode);
}



/*
 * Return the current position.
 */
char **
histpos()
{
	return current;
}

int
histN()
{
	return curpos;
}

int
histnum(n)
{
	int	last = histptr - history;

	if (n < 0 || n >= last) {
		current = histptr;
		curpos = last;
		return last;
	}  else {
		current = &history[n];
		curpos = n;
		return n;
	}
}

/*
 * This will become unecessary if histget is modified to allow
 * searching from positions other than the end, and in either 
 * direction.
 */
char *
findhist(start, fwd, str)
	int	start;
	int	fwd;
	char 	*str;
{
	int	 pos = start;
	char	 *line, *last;

	/* XXX check that we are valid after this */
	if (fwd)
		pos++;
	else
		pos--;
	histnum(pos);
	line = *histpos();
	do {
		last = line;
		if (strstr(line, str) != 0) {
			/* keep position current */
			return (line);
		}
		if (fwd)
			pos++;
		else
			pos--;
		histnum(pos);
		line = *histpos();
	} while (line && *line && line != last && pos>0);

	histnum(start);
	if (pos <= 0)
		return (char*)-1; /* TODO */
	return NULL;
}
