/*
 *	vi command editing
 *	written by John Rochester (initially for nsh)
 *	bludgeoned to fit PD ksh by Larry Bouzane and Eric Gisin
 *	Further hacked (bugfixes and tweaks) by Mike Jetzer
 */

#include "config.h"
#ifdef VI

#ifndef lint
static char *RCSid = "$Id: vi.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include "sh.h"
#include "expand.h"
#include "edit.h"

#define CMDLEN		256
#define Ctrl(c)		(c&0x1f)
#define	bcopy(src, dst, len)	memmove(dst, src, len)
extern	int	histN();

static int      nextstate   ARGS((int ch));
static int      vi_insert   ARGS((int ch));
static int      vi_cmd      ARGS((int argcnt, char *cmd));
static int      domove      ARGS((int argcnt, char *cmd, int sub));
static int      redo_insert ARGS((int count));
static          yank_range  ARGS((int a, int b));
static int      bracktype   ARGS((int ch));
static          edit_reset  ARGS((char *buf, int len));
static int      putbuf      ARGS((char *buf, int len, int repl));
static          stripblanks ARGS((void));
static          del_range   ARGS((int a, int b));
static int      findch      ARGS((int ch, int cnt, int forw, int incl));
static int      forwword    ARGS((int argcnt));
static int      backword    ARGS((int argcnt));
static int      endword     ARGS((int argcnt));
static int      Forwword    ARGS((int argcnt));
static int      Backword    ARGS((int argcnt));
static int      Endword     ARGS((int argcnt));
static int      grabhist    ARGS((int save, int n));
static int      grabsearch  ARGS((int save, int start, int fwd, char *pat));
static          redraw_line ARGS((void));
static          refresh     ARGS((int leftside));
static int      outofwin    ARGS((void));
static          rewindow    ARGS((void));
static int      newcol      ARGS((int ch, int col));
static          display     ARGS((char *wb1, char *wb2, int leftside));
static          ed_mov_opt  ARGS((int col, char *wb));

#define C_	0x1
#define M_	0x2
#define E_	0x4
#define X_	0x8
#define U_	0x10
#define B_	0x20
#define O_	0x40
#define S_	0x80

#define isbad(c)	(classify[c]&B_)
#define iscmd(c)	(classify[c]&(M_|E_|C_|U_))
#define ismove(c)	(classify[c]&M_)
#define isextend(c)	(classify[c]&E_)
#define islong(c)	(classify[c]&X_)
#define ismeta(c)	(classify[c]&O_)
#define isundoable(c)	(!(classify[c]&U_))
#define issrch(c)	(classify[c]&S_)

char	classify[128] = {
	B_,	0,	0,	0,	0,	0,	O_,	0,
#if 1	/* Mike B. changes */
	C_|M_,	0,	O_,	0,	O_,	O_,	O_,	0,
#else
	C_,	0,	O_,	0,	O_,	O_,	O_,	0,
#endif
	O_,	0,	C_|U_,	0,	0,	0,	0,	0,
	0,	0,	O_,	0,	0,	0,	0,	0,
#if 1	/* Mike B. changes */
	C_|M_,	0,	0,	C_,	M_,	C_,	0,	0,
#else
	C_,	0,	0,	C_,	M_,	C_,	0,	0,
#endif
	0,	0,	C_,	C_,	M_,	C_,	0,	C_|S_,
	M_,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	M_,	0,	0,	0,	C_|S_,
	0,	C_,	M_,	C_,	C_,	M_,	M_|X_,	C_,
	0,	C_,	0,	0,	0,	0,	C_,	0,
	C_,	0,	C_,	C_,	M_|X_,	0,	0,	M_,
	C_,	C_,	0,	0,	0,	0,	M_,	C_,
	0,	C_,	M_,	E_,	E_,	M_,	M_|X_,	0,
	M_,	C_,	C_,	C_,	M_,	0,	C_,	0,
	C_,	0,	C_,	C_,	M_|X_,	C_|U_,	0,	M_,
	C_,	E_,	0,	0,	0,	0,	C_,	0
};

#define MAXVICMD	3
#define SRCHLEN		40

#define INSERT		1
#define REPLACE		2

#define VNORMAL		0
#define VARG1		1
#define VEXTCMD		2
#define VARG2		3
#define VXCH		4
#define VFAIL		5
#define VCMD		6
#define VREDO		7
#define VLIT		8
#define VSEARCH		9
#define VREPLACE1CHAR	10

struct edstate {
	int	winleft;
	char	*cbuf;
	int	cbufsize;
	int	linelen;
	int	cursor;
};

static char		undocbuf[CMDLEN];

static struct edstate	ebuf;
static struct edstate	undobuf = { 0, undocbuf, CMDLEN, 0, 0 };

static struct edstate	*es;			/* current editor state */
static struct edstate	*undo;

static char	ibuf[CMDLEN];		/* input buffer */
static int	inslen;			/* length of input buffer */
static int	srchlen;		/* length of current search pattern */
static char	ybuf[CMDLEN];		/* yank buffer */
static int	yanklen;		/* length of yank buffer */
static int	fsavecmd = ' ';		/* last find command */
static int	fsavech;		/* character to find */
static char	lastcmd[MAXVICMD];	/* last non-move command */
static int	lastac;			/* argcnt for lastcmd */
static int	lastsearch = ' ';	/* last search command */
static char	srchpat[SRCHLEN];	/* last search pattern */
static int	insert;			/* non-zero in insert mode */
static int	hnum;			/* position in history */
static int	hlast;			/* 1 past last position in history */
static int	modified;		/* buffer has been "modified" */
static int	state;

#if 0
vi_init()
{
	es = (struct edstate *) malloc((unsigned) sizeof(struct edstate));
	fsavecmd = ' ';
	lastsearch = ' ';
}

edit_init()
{
	wbuf[0] = malloc((unsigned) (x_cols - 3));
	wbuf[1] = malloc((unsigned) (x_cols - 3));
}
#endif

void
vi_reset(buf, len)
	char	*buf;
	int	len;
{
	state = VNORMAL;
	hnum = hlast = histnum(-1) + 1;
	insert = INSERT;
	yanklen = 0;
	inslen = 0;
	lastcmd[0] = 'a';
	lastac = 1;
	modified = 1;
	edit_reset(buf, len);
}

int
vi_hook(ch)
	int		ch;
{
	static char	curcmd[MAXVICMD];
	static char	locpat[SRCHLEN];
	static int	cmdlen;
	static int	argc1, argc2;

	if (state != VSEARCH && (ch == '\r' || ch == '\n')) {
		x_putc('\r');
		x_putc('\n');
		x_flush();
		return 1;
	}

	switch (state) {

	case VREPLACE1CHAR:
		curcmd[cmdlen++] = ch;
		state = VCMD;
		break;

	case VNORMAL:
		if (insert != 0) {
			if (ch == Ctrl('v')) {
				state = VLIT;
				ch = '^';
			}
			if (vi_insert(ch) != 0) {
				x_putc(Ctrl('g'));
				state = VNORMAL;
			} else {
				if (state == VLIT) {
					es->cursor--;
					refresh(0);
				} else
					refresh(insert != 0);
			}
		} else {
			cmdlen = 0;
			argc1 = 0;
			if (ch >= '1' && ch <= '9') {
				argc1 = ch - '0';
				state = VARG1;
			} else {
				curcmd[cmdlen++] = ch;
				state = nextstate(ch);
				if (state == VSEARCH) {
					save_cbuf();
					es->cursor = 0;
					es->linelen = 0;
					if (ch == '/') {
						if (putbuf("/", 1, 0) != 0) {
							return -1;
						}
					} else if (putbuf("?", 1, 0) != 0) 
							return -1;
					refresh(0);
				}
			}
		}
		break;

	case VLIT:
		if (isbad(ch)) {
			del_range(es->cursor, es->cursor + 1);
			x_putc(Ctrl('g'));
		} else
			es->cbuf[es->cursor++] = ch;
		refresh(1);
		state = VNORMAL;
		break;

	case VARG1:
		if (isdigit(ch))
			argc1 = argc1 * 10 + ch - '0';
		else {
			curcmd[cmdlen++] = ch;
			state = nextstate(ch);
		}
		break;

	case VEXTCMD:
		argc2 = 0;
		if (ch >= '1' && ch <= '9') {
			argc2 = ch - '0';
			state = VARG2;
			return 0;
		} else {
			curcmd[cmdlen++] = ch;
			if (ch == curcmd[0])
				state = VCMD;
			else if (ismove(ch))
				state = nextstate(ch);
			else
				state = VFAIL;
		}
		break;

	case VARG2:
		if (isdigit(ch))
			argc2 = argc2 * 10 + ch - '0';
		else {
			if (argc1 == 0)
				argc1 = argc2;
			else
				argc1 *= argc2;
			curcmd[cmdlen++] = ch;
			if (ch == curcmd[0])
				state = VCMD;
			else if (ismove(ch))
				state = nextstate(ch);
			else
				state = VFAIL;
		}
		break;

	case VXCH:
		if (ch == Ctrl('['))
			state = VNORMAL;
		else {
			curcmd[cmdlen++] = ch;
			state = VCMD;
		}
		break;

	case VSEARCH:
		switch (ch) {

		/* case Ctrl('['): */
		case '\r':
		case '\n':
			locpat[srchlen] = '\0';
			(void) strcpy(srchpat, locpat);
			/* redraw_line(); */
			state = VCMD;
			break;

		case 0x7f:
			if (srchlen == 0) {
				restore_cbuf();
				state = VNORMAL;
			} else {
				srchlen--;
				if (locpat[srchlen] < ' ' ||
						locpat[srchlen] == 0x7f) {
					es->linelen--;
				}
				es->linelen--;
				es->cursor = es->linelen;
				refresh(0);
				return 0;
			}
			refresh(0);
			break;

		case Ctrl('u'):
			srchlen = 0;
			es->linelen = 1;
			es->cursor = 1;
			refresh(0);
			return 0;

		default:
			if (srchlen == SRCHLEN - 1)
				x_putc(Ctrl('g'));
			else {
				locpat[srchlen++] = ch;
				if (ch < ' ' || ch == 0x7f) {
					es->cbuf[es->linelen++] = '^';
					es->cbuf[es->linelen++] = ch ^ '@';
				} else
					es->cbuf[es->linelen++] = ch;
				es->cursor = es->linelen;
				refresh(0);
			}
			return 0;
			break;
		}
		break;
	}
	switch (state) {

	case VCMD:
		state = VNORMAL;
		switch (vi_cmd(argc1, curcmd)) {
		case -1:
			x_putc(Ctrl('g'));
			break;
		case 0:
			if (insert != 0)
				inslen = 0;
			refresh(insert != 0);
			break;
		case 1:
			refresh(0);
			x_putc('\r');
			x_putc('\n');
			x_flush();
			return 1;
		}
		break;

	case VREDO:
		state = VNORMAL;
		if (argc1 != 0)
			lastac = argc1;
		switch (vi_cmd(lastac, lastcmd) != 0) {
		case -1:
			x_putc(Ctrl('g'));
			refresh(0);
			break;
		case 0:
			if (insert != 0) {
				if (lastcmd[0] == 's' || lastcmd[0] == 'c' ||
						lastcmd[0] == 'C') {
					if (redo_insert(1) != 0)
						x_putc(Ctrl('g'));
				} else {
					if (redo_insert(lastac) != 0)
						x_putc(Ctrl('g'));
				}
			}
			refresh(0);
			break;
		case 1:
			refresh(0);
			x_putc('\r');
			x_putc('\n');
			x_flush();
			return 1;
		}
		break;

	case VFAIL:
		state = VNORMAL;
		x_putc(Ctrl('g'));
		break;
	}
	return 0;
}

static int
nextstate(ch)
	int	ch;
{
	/*
	 * probably could have been done more elegantly than
	 * by creating a new state, but it works
	 */
	if (ch == 'r')
		return VREPLACE1CHAR;
	else if (isextend(ch))
		return VEXTCMD;
	else if (issrch(ch))
		return VSEARCH;
	else if (islong(ch))
		return VXCH;
	else if (ch == '.')
		return VREDO;
	else if (iscmd(ch))
		return VCMD;
	else
		return VFAIL;
}

static int
vi_insert(ch)
	int	ch;
{
	int	tcursor;

	switch (ch) {

	case '\0':
		return -1;

	case Ctrl('['):
		if (lastcmd[0] == 's' || lastcmd[0] == 'c' ||
				lastcmd[0] == 'C')
			return redo_insert(0);
		else
			return redo_insert(lastac - 1);

	case 0x7f:		/* delete */
		/* tmp fix */
		/* general fix is to get stty erase char and use that
		*/
	case Ctrl('H'):		/* delete */
		if (es->cursor != 0) {
			if (inslen > 0)
				inslen--;
			es->cursor--;
			if (insert != REPLACE) {
				bcopy(&es->cbuf[es->cursor+1],
						&es->cbuf[es->cursor],
						es->linelen - es->cursor);
				es->linelen--;
			}
		}
		break;

	case Ctrl('U'):
		if (es->cursor != 0) {
			inslen = 0;
			bcopy(&es->cbuf[es->cursor], es->cbuf,
						es->linelen - es->cursor);
			es->linelen -= es->cursor;
			es->cursor = 0;
		}
		break;

	case Ctrl('W'):
		if (es->cursor != 0) {
			tcursor = backword(1);
			bcopy(&es->cbuf[es->cursor], &es->cbuf[tcursor],
						es->linelen - es->cursor);
			es->linelen -= es->cursor - tcursor;
			if (inslen < es->cursor - tcursor)
				inslen = 0;
			else
				inslen -= es->cursor - tcursor;
			es->cursor = tcursor;
		}
		break;

	default:
		if (es->linelen == es->cbufsize - 1)
			return -1;
		ibuf[inslen++] = ch;
		if (insert == INSERT) {
			bcopy(&es->cbuf[es->cursor], &es->cbuf[es->cursor+1],
					es->linelen - es->cursor);
			es->linelen++;
		}
		es->cbuf[es->cursor++] = ch;
		if (insert == REPLACE && es->cursor > es->linelen)
			es->linelen++;
	}
	return 0;
}

static int
vi_cmd(argcnt, cmd)
	int		argcnt;
	char		*cmd;
{
	int		ncursor;
	int		cur, c1, c2, c3 = 0;
	struct edstate	*t;


	if (argcnt == 0) {
		if (*cmd == 'G')
			argcnt = hlast + 1;
		else if (*cmd != '_')
			argcnt = 1;
	}

	if (ismove(*cmd)) {
		if ((cur = domove(argcnt, cmd, 0)) >= 0) {
			if (cur == es->linelen && cur != 0)
				cur--;
			es->cursor = cur;
		} else
			return -1;
	} else {
		if (isundoable(*cmd)) {
			undo->winleft = es->winleft;
			bcopy(es->cbuf, undo->cbuf, es->linelen);
			undo->linelen = es->linelen;
			undo->cursor = es->cursor;
			lastac = argcnt;
			bcopy(cmd, lastcmd, MAXVICMD);
		}
		switch (*cmd) {

		case Ctrl('r'):
			redraw_line();
			break;

		case 'a':
			modified = 1;
			if (es->linelen != 0)
				es->cursor++;
			insert = INSERT;
			break;

		case 'A':
			modified = 1;
			del_range(0, 0);
			es->cursor = es->linelen;
			insert = INSERT;
			break;

		case 'c':
		case 'd':
		case 'y':
			if (*cmd == cmd[1]) {
				c1 = 0;
				c2 = es->linelen;
			} else if (!ismove(cmd[1]))
				return -1;
			else {
				if ((ncursor = domove(argcnt, &cmd[1], 1)) < 0)
					return -1;
				if (*cmd == 'c' &&
						(cmd[1]=='w' || cmd[1]=='W') &&
						!isspace(es->cbuf[es->cursor])) {
					while (isspace(es->cbuf[--ncursor]))
						;
					ncursor++;
				}
				if (ncursor > es->cursor) {
					c1 = es->cursor;
					c2 = ncursor;
				} else {
					c1 = ncursor;
					c2 = es->cursor;
				}
			}
			if (*cmd != 'c' && c1 != c2)
				yank_range(c1, c2);
			if (*cmd != 'y') {
				del_range(c1, c2);
				es->cursor = c1;
			}
			if (*cmd == 'c') {
				modified = 1;
				insert = INSERT;
			}
			break;

		case 'p':
			modified = 1;
			if (es->linelen != 0)
				es->cursor++;
			while (putbuf(ybuf, yanklen, 0) == 0 && --argcnt > 0)
				;
			if (es->cursor != 0)
				es->cursor--;
			if (argcnt != 0)
				return -1;
			break;

		case 'P':
			modified = 1;
			while (putbuf(ybuf, yanklen, 0) == 0 && --argcnt > 0)
				;
			if (es->cursor != 0)
				es->cursor--;
			if (argcnt != 0)
				return -1;
			break;

		case 'C':
			modified = 1;
			del_range(es->cursor, es->linelen);
			insert = INSERT;
			break;

		case 'D':
			yank_range(es->cursor, es->linelen);
			del_range(es->cursor, es->linelen);
			if (es->cursor != 0)
				es->cursor--;
			break;

		case 'G':
			if (grabhist(modified, argcnt - 1) < 0)
				return -1;
			else {
				modified = 0;
				hnum = argcnt - 1;
			}
			break;

		case 'i':
			modified = 1;
			insert = INSERT;
			break;

		case 'I':
			modified = 1;
			es->cursor = 0;
			insert = INSERT;
			break;

		case '+':
		case 'j':
			if (grabhist(modified, hnum + argcnt) < 0)
				return -1;
			else {
				modified = 0;
				hnum += argcnt;
			}
			break;

		case '-':
		case 'k':
			if (grabhist(modified, hnum - argcnt) < 0)
				return -1;
			else {
				modified = 0;
				hnum -= argcnt;
			}
			break;

		case 'r':
			if (es->linelen == 0)
				return -1;
			modified = 1;
			es->cbuf[es->cursor] = cmd[1];
			break;

		case 'R':
			modified = 1;
			insert = REPLACE;
			break;

		case 's':
			if (es->linelen == 0)
				return -1;
			modified = 1;
			if (es->cursor + argcnt > es->linelen)
				argcnt = es->linelen - es->cursor;
			del_range(es->cursor, es->cursor + argcnt);
			insert = INSERT;
			break;

		case 'x':
			if (es->linelen == 0)
				return -1;
			modified = 1;
			if (es->cursor + argcnt > es->linelen)
				argcnt = es->linelen - es->cursor;
			yank_range(es->cursor, es->cursor + argcnt);
			del_range(es->cursor, es->cursor + argcnt);
			break;

		case 'X':
			if (es->cursor > 0) {
				modified = 1;
				if (es->cursor < argcnt)
					argcnt = es->cursor;
				yank_range(es->cursor - argcnt, es->cursor);
				del_range(es->cursor - argcnt, es->cursor);
				es->cursor -= argcnt;
			} else
				return -1;
			break;

		case 'u':
			t = es;
			es = undo;
			undo = t;
			break;

		case '?':
			hnum = -1;
			/* ahhhhhh... */
		case '/':
			c3 = 1;
			srchlen = 0;
			lastsearch = *cmd;
			/* fall through */
		case 'n':
		case 'N':
			if (lastsearch == ' ')
				return -1;
			if (lastsearch == '?')
				c1 = 1; 
			else
				c1 = 0;
			if (*cmd == 'N')
				c1 = !c1;
			if ((c2 = grabsearch(modified, hnum,
							c1, srchpat)) < 0) {
				if (c3) {
					restore_cbuf();
					refresh(0);
				}
				return -1;
			} else {
				modified = 0;
				hnum = c2;
			}
			break;
		case '_': {
			int	space;
			char	*p, *sp;

			(void) histnum(-1);
			p = *histpos();
#define issp(c)		(isspace((c)) || (c) == '\n')
			if (argcnt) {
				while (*p && issp(*p))
					p++;
				while (*p && --argcnt) {
					while (*p && !issp(*p))
						p++;
					while (*p && issp(*p))
						p++;
				}
				if (!*p)
					return -1;
				sp = p;
			} else {
				sp = p;
				space = 0;
				while (*p) {
					if (issp(*p))
						space = 1;
					else if (space) {
						space = 0;
						sp = p;
					}
					p++;
				}
				p = sp;
			}
			modified = 1;
			if (es->linelen != 0)
				es->cursor++;
			while (*p && !issp(*p)) {
				argcnt++;
				p++;
			}
			if (putbuf(" ", 1, 0) != 0)
				argcnt = -1;
			else if (putbuf(sp, argcnt, 0) != 0)
				argcnt = -1;
			if (argcnt < 0) {
				if (es->cursor != 0)
					es->cursor--;
				return -1;
			}
			insert = INSERT;
			}
			break;

		case '~': {
			char	*p;

			if (es->linelen == 0)
				return -1;
			p = &es->cbuf[es->cursor];
			if (islower(*p)) {
				modified = 1;
				*p = toupper(*p);
			} else if (isupper(*p)) {
				modified = 1;
				*p = tolower(*p);
			}
			if (es->cursor < es->linelen - 1)
				es->cursor++;
			}
			break;

		case '#':
			es->cursor = 0;
			if (putbuf("#", 1, 0) != 0)
				return -1;
			return 1;

		case '*': {
			int	rval = 0;
			int	start, end;
			char	*toglob = undo->cbuf;
			char	**ap;
			char	**ap2;
			char	**globstr();

			if (isspace(es->cbuf[es->cursor]))
				return -1;
			start = es->cursor;
			while (start > -1 && !isspace(es->cbuf[start]))
				start--;
			start++;
			end = es->cursor;
			while (end < es->linelen && !isspace(es->cbuf[end]))
				end++;
			/* use undo buffer to build word up in */
			bcopy(&es->cbuf[start], toglob, end-start);
			if (*toglob != '~' && toglob[end-start-1] != '*') {
				toglob[end-start] = '*';
				toglob[end-start+1] = '\0';
			} else
				toglob[end-start] = '\0';
			ap = globstr(toglob);
			ap2 = ap;
			if (strcmp(ap[0], toglob) == 0 && ap[1] == (char *) 0)
				rval = -1;
			/* restore undo buffer that we used temporarily */
			bcopy(es->cbuf, toglob, es->linelen);
			if (rval < 0)
				return rval;
			del_range(start, end);
			es->cursor = start;
			while (1) {
				if (putbuf(*ap, strlen(*ap), 0) != 0) {
					rval = -1;
					break;
				}
				if (*++ap == (char *) 0)
					break;
				if (putbuf(" ", 1, 0) != 0) {
					rval = -1;
					break;
				}
			}
#if 0
			/*
			 * this is definitely wrong
			 */
			for (ap = ap2; *ap; ap++)
			    free(*ap);

			free(ap2);
#endif

			modified = 1;
			insert = INSERT;
			refresh(0);
			if (rval != 0)
				return rval;
			}
			break;
		}
		if (insert == 0 && es->cursor != 0 && es->cursor >= es->linelen)
			es->cursor--;
	}
	return 0;
}

static int
domove(argcnt, cmd, sub)
	int	argcnt;
	char	*cmd;
	int	sub;
{
	int	bcount, i = 0, t;	/* = 0 kludge for gcc -W */
	int	ncursor = 0;		/* = 0 kludge for gcc -W */

	switch (*cmd) {

	case 'b':
		if (!sub && es->cursor == 0)
			return -1;
		ncursor = backword(argcnt);
		break;

	case 'B':
		if (!sub && es->cursor == 0)
			return -1;
		ncursor = Backword(argcnt);
		break;

	case 'e':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		ncursor = endword(argcnt);
		if (sub)
			ncursor++;
		break;

	case 'E':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		ncursor = Endword(argcnt);
		if (sub)
			ncursor++;
		break;

	case 'f':
	case 'F':
	case 't':
	case 'T':
		fsavecmd = *cmd;
		fsavech = cmd[1];
		/* drop through */

	case ',':
	case ';':
		if (fsavecmd == ' ')
			return -1;
		i = fsavecmd == 'f' || fsavecmd == 'F';
		t = fsavecmd > 'a';
		if (*cmd == ',')
			t = !t;
		if ((ncursor = findch(fsavech, argcnt, t, i)) < 0)
			return -1;
		if (sub && t)
			ncursor++;
		break;

	case 'h':
		/* tmp fix */
	case Ctrl('H'):
		if (!sub && es->cursor == 0)
			return -1;
		ncursor = es->cursor - argcnt;
		if (ncursor < 0)
			ncursor = 0;
		break;

	case ' ':
	case 'l':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		if (es->linelen != 0) {
			ncursor = es->cursor + argcnt;
			if (ncursor >= es->linelen)
				ncursor = es->linelen - 1;
		}
		break;

	case 'w':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		ncursor = forwword(argcnt);
		break;

	case 'W':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		ncursor = Forwword(argcnt);
		break;

	case '0':
		ncursor = 0;
		break;

	case '^':
		ncursor = 0;
		while (ncursor < es->linelen - 1 && isspace(es->cbuf[ncursor]))
			ncursor++;
		break;

	case '$':
		if (es->linelen != 0)
			ncursor = es->linelen;
		else
			ncursor = 0;
		break;

	case '%':
		ncursor = es->cursor;
		while (ncursor < es->linelen &&
				(i = bracktype(es->cbuf[ncursor])) == 0)
			ncursor++;
		if (ncursor == es->linelen)
			return -1;
		bcount = 1;
		do {
			if (i > 0) {
				if (++ncursor >= es->linelen)
					return -1;
			} else {
				if (--ncursor < 0)
					return -1;
			}
			t = bracktype(es->cbuf[ncursor]);
			if (t == i)
				bcount++;
			else if (t == -i)
				bcount--;
		} while (bcount != 0);
		if (sub)
			ncursor++;
		break;

	default:
		return -1;
	}
	return ncursor;
}

static int
redo_insert(count)
	int	count;
{
	while (count-- > 0)
		if (putbuf(ibuf, inslen, insert==REPLACE) != 0)
			return -1;
	if (es->cursor > 0)
		es->cursor--;
	insert = 0;
	return 0;
}

static
yank_range(a, b)
	int	a, b;
{
	yanklen = b - a;
	if (yanklen != 0)
		bcopy(&es->cbuf[a], ybuf, yanklen);
}

static int
bracktype(ch)
	int	ch;
{
	switch (ch) {

	case '(':
		return 1;

	case '[':
		return 2;

	case '{':
		return 3;

	case ')':
		return -1;

	case ']':
		return -2;

	case '}':
		return -3;

	default:
		return 0;
	}
}

/*
 *	Non user interface editor routines below here
 */

static int	cur_col;		/* current column on line */
static int	pwidth;			/* width of prompt */
static int	winwidth;		/* width of window */
/*static char	*wbuf[2];		/* window buffers */
static char	wbuf[2][80-3];	/* window buffers */ /* TODO */
static int	win;			/* window buffer in use */
static char	morec;			/* more character at right of window */
static int	lastref;		/* argument to last refresh() */
static char	holdbuf[CMDLEN];	/* place to hold last edit buffer */
static int	holdlen;		/* length of holdbuf */

save_cbuf()
{
	bcopy(es->cbuf, holdbuf, es->linelen);
	holdlen = es->linelen;
	holdbuf[holdlen] = '\0';
}

restore_cbuf()
{
	es->cursor = 0;
	es->linelen = holdlen;
	bcopy(holdbuf, es->cbuf, holdlen);
}

static
edit_reset(buf, len)
	char	*buf;
	int	len;
{
	es = &ebuf;
	es->cbuf = buf;
	es->cbufsize = len;
	undo = &undobuf;
	undo->cbufsize = len;

	es->linelen = undo->linelen = 0;
	es->cursor = undo->cursor = 0;
	es->winleft = undo->winleft = 0;

	cur_col = pwidth = promptlen(prompt);
	winwidth = x_cols - pwidth - 3;
	x_putc('\r');
	x_flush();
	pprompt(prompt);
	/* docap(CLR_EOL, 0); */
	win = 0;
	morec = ' ';
	lastref = 1;
}

static int
putbuf(buf, len, repl)
	char	*buf;
	int	len;
	int	repl;
{
	if (len == 0)
		return 0;
	if (repl) {
		if (es->cursor + len >= es->cbufsize - 1)
			return -1;
		if (es->cursor + len > es->linelen)
			es->linelen = es->cursor + len;
	} else {
		if (es->linelen + len >= es->cbufsize - 1)
			return -1;
		bcopy(&es->cbuf[es->cursor], &es->cbuf[es->cursor + len],
			es->linelen - es->cursor);
		es->linelen += len;
	}
	bcopy(buf, &es->cbuf[es->cursor], len);
	es->cursor += len;
	return 0;
}

static
stripblanks()
{
	int	ncursor;

	ncursor = 0;
	while (ncursor < es->linelen && isspace(es->cbuf[ncursor]))
		ncursor++;
	del_range(0, ncursor);
}

static
del_range(a, b)
	int	a, b;
{
	if (es->linelen != b)
		bcopy(&es->cbuf[b], &es->cbuf[a], es->linelen - b);
	es->linelen -= b - a;
}

static int
findch(ch, cnt, forw, incl)
	int	ch;
	int	forw;
	int	incl;
{
	int	ncursor;

	if (es->linelen == 0)
		return -1;
	ncursor = es->cursor;
	while (cnt--) {
		do {
			if (forw) {
				if (++ncursor == es->linelen)
					return -1;
			} else {
				if (--ncursor < 0)
					return -1;
			}
		} while (es->cbuf[ncursor] != ch);
	}
	if (!incl) {
		if (forw)
			ncursor--;
		else
			ncursor++;
	}
	return ncursor;
}

#define Isalnum(x) (isalnum(x) || (x == '_'))
static int
forwword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor < es->linelen && argcnt--) {
		if (Isalnum(es->cbuf[ncursor]))
			while (Isalnum(es->cbuf[ncursor]) &&
					++ncursor < es->linelen)
				;
		else if (!isspace(es->cbuf[ncursor]))
			while (!Isalnum(es->cbuf[ncursor]) &&
					!isspace(es->cbuf[ncursor]) &&
					++ncursor < es->linelen)
				;
		while (isspace(es->cbuf[ncursor]) && ++ncursor < es->linelen)
			;
	}
	return ncursor;
}

static int
backword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor > 0 && argcnt--) {
		while (--ncursor > 0 && isspace(es->cbuf[ncursor]))
			;
		if (ncursor > 0) {
			if (Isalnum(es->cbuf[ncursor]))
				while (--ncursor >= 0 &&
				   Isalnum(es->cbuf[ncursor]))
					;
			else
				while (--ncursor >= 0 &&
				   !Isalnum(es->cbuf[ncursor]) &&
				   !isspace(es->cbuf[ncursor]))
					;
			ncursor++;
		}
	}
	return ncursor;
}

static int
endword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor < es->linelen && argcnt--) {
		while (++ncursor < es->linelen - 1 &&
				isspace(es->cbuf[ncursor]))
			;
		if (ncursor < es->linelen - 1) {
			if (Isalnum(es->cbuf[ncursor]))
				while (++ncursor < es->linelen &&
					  Isalnum(es->cbuf[ncursor]))
					;
			else
				while (++ncursor < es->linelen &&
				   !Isalnum(es->cbuf[ncursor]) &&
				   !isspace(es->cbuf[ncursor]))
					;
			ncursor--;
		}
	}
	return ncursor;
}

static int
Forwword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor < es->linelen && argcnt--) {
		while (!isspace(es->cbuf[ncursor]) && ++ncursor < es->linelen)
			;
		while (isspace(es->cbuf[ncursor]) && ++ncursor < es->linelen)
			;
	}
	return ncursor;
}

static int
Backword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor > 0 && argcnt--) {
		while (--ncursor >= 0 && isspace(es->cbuf[ncursor]))
			;
		while (ncursor >= 0 && !isspace(es->cbuf[ncursor]))
			ncursor--;
		ncursor++;
	}
	return ncursor;
}

static int
Endword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor < es->linelen - 1 && argcnt--) {
		while (++ncursor < es->linelen - 1 &&
				isspace(es->cbuf[ncursor]))
			;
		if (ncursor < es->linelen - 1) {
			while (++ncursor < es->linelen &&
					!isspace(es->cbuf[ncursor]))
				;
			ncursor--;
		}
	}
	return ncursor;
}

static int
grabhist(save, n)
	int	save;
	int	n;
{
	char	*hptr;

	if (n < 0 || n > hlast)
		return -1;
	if (n == hlast) {
		restore_cbuf();
		return 0;
	}
	(void) histnum(n);
	if ((hptr = *histpos()) == NULL) {
		shellf("grabhist: bad history array\n");
		return -1;
	}
	if (save)
		save_cbuf();
	es->linelen = strlen(hptr);
	bcopy(hptr, es->cbuf, es->linelen);
	es->cursor = 0;
	return 0;
}

static int
grabsearch(save, start, fwd, pat)
	int	save, start, fwd;
	char	*pat;
{
	char	*hptr;

	if ((start == 0 && fwd == 0) || (start >= hlast - 1 && fwd == 1))
		return -1;
	if ((hptr = findhist(start, fwd, pat)) == NULL) {
		/* if (start != 0 && fwd && match(holdbuf, pat) >= 0) { */
		if (start != 0 && fwd && strcmp(holdbuf, pat) >= 0) {
			restore_cbuf();
			return 0;
		} else
			return -1;
	} else if (hptr == (char *)-1) {
		return -1;
	}
	if (save)
		save_cbuf();
	es->linelen = strlen(hptr);
	bcopy(hptr, es->cbuf, es->linelen);
	es->cursor = 0;
	return histN();
}

static
redraw_line()
{
	x_putc('\r');
	x_putc('\n');
	x_flush();
	pprompt(prompt);
	cur_col = 2;
	morec = ' ';
}

static
refresh(leftside)
	int		leftside;
{
	if (leftside < 0)
		leftside = lastref;
	else
		lastref = leftside;
	if (outofwin())
		rewindow();
	display(wbuf[1 - win], wbuf[win], leftside);
	win = 1 - win;
}

static int
outofwin()
{
	int	cur, col;

	if (es->cursor < es->winleft)
		return 1;
	col = 0;
	cur = es->winleft;
	while (cur < es->cursor)
		col = newcol(es->cbuf[cur++], col);
	if (col > winwidth)
		return 1;
	return 0;
}

static
rewindow()
{
	register int	tcur, tcol;
	int		holdcur1, holdcol1;
	int		holdcur2, holdcol2;

	holdcur1 = holdcur2 = tcur = 0;
	holdcol1 = holdcol2 = tcol = 0;
	while (tcur < es->cursor) {
		if (tcol - holdcol2 > winwidth / 2) {
			holdcur1 = holdcur2;
			holdcol1 = holdcol2;
			holdcur2 = tcur;
			holdcol2 = tcol;
		}
		tcol = newcol(es->cbuf[tcur++], tcol);
	}
	while (tcol - holdcol1 > winwidth / 2)
		holdcol1 = newcol(es->cbuf[holdcur1++], holdcol1);
	es->winleft = holdcur1;
}

static int
newcol(ch, col)
	int	ch, col;
{
	if (ch < ' ' || ch == 0x7f) {
		if (ch == '\t')
			return (col | 7) + 1;
		else
			return col + 2;
	} else
		return col + 1;
}

static
display(wb1, wb2, leftside)
	char	*wb1, *wb2;
	int	leftside;
{
	char	*twb1, *twb2, mc;
	int	cur, col, cnt;
	int	ncol = 0; /* set to 0 kludge for gcc -W */
	int	moreright;

	col = 0;
	cur = es->winleft;
	moreright = 0;
	twb1 = wb1;
	while (col < winwidth && cur < es->linelen) {
		if (cur == es->cursor && leftside)
			ncol = col + pwidth;
		if (es->cbuf[cur] < ' ' || es->cbuf[cur] == 0x7f) {
			if (es->cbuf[cur] == '\t') {
				do {
					*twb1++ = ' ';
				} while (++col < winwidth && (col & 7) != 0);
			} else {
				*twb1++ = '^';
				if (++col < winwidth) {
					*twb1++ = es->cbuf[cur] ^ '@';
					col++;
				}
			}
		} else {
			*twb1++ = es->cbuf[cur];
			col++;
		}
		if (cur == es->cursor && !leftside)
			ncol = col + pwidth - 1;
		cur++;
	}
	if (cur == es->cursor)
		ncol = col + pwidth;
	if (col < winwidth) {
		while (col < winwidth) {
			*twb1++ = ' ';
			col++;
		}
	} else
		moreright++;
	*twb1 = ' ';

	col = pwidth;
	cnt = winwidth;
	twb1 = wb1;
	twb2 = wb2;
	while (cnt--) {
		if (*twb1 != *twb2) {
			if (cur_col != col)
				ed_mov_opt(col, wb1);
			x_putc(*twb1);
			cur_col++;
		}
		twb1++;
		twb2++;
		col++;
	}
	if (es->winleft > 0 && moreright)
		mc = '+';
	else if (es->winleft > 0)
		mc = '<';
	else if (moreright)
		mc = '>';
	else
		mc = ' ';
	if (mc != morec) {
		ed_mov_opt(x_cols - 2, wb1);
		x_putc(mc);
		cur_col++;
		morec = mc;
	}
#if 0
	/*
	 * Hack to fix teh ^r redraw problem, but it redraws way too much.
	 * Probably unacceptable at low baudrates.  Someone please fix this
	 */
	else
	    {
	    ed_mov_opt(x_cols - 2, wb1);
	    }
#endif
	if (cur_col != ncol)
		ed_mov_opt(ncol, wb1);
}

static
ed_mov_opt(col, wb)
	int	col;
	char	*wb;
{
	if (col < cur_col) {
		if (col + 1 < cur_col - col) {
			x_putc('\r');
			x_flush();
			pprompt(prompt);
			cur_col = pwidth;
			while (cur_col++ < col)
				x_putc(*wb++);
		} else {
			while (cur_col-- > col)
				x_putc('\b');
		}
	} else {
		wb = &wb[cur_col - pwidth];
		while (cur_col++ < col)
			x_putc(*wb++);
	}
	cur_col = col;
}

int
x_vi(buf, len)
	char *buf;
	size_t len;
{
	int	c;

	vi_reset(buf, len > CMDLEN ? CMDLEN : len);
	x_flush();
	while ((c = getch()) != -1) {
		if (vi_hook(c))
			break;
		x_flush();
	}

	if (c == -1)
		return -1;

	if (es->cbuf != buf) {
		bcopy(es->cbuf, buf, es->linelen);
		buf[es->linelen] = '\n';
	} else
		es->cbuf[es->linelen] = '\n';

	es->linelen++;
	return es->linelen;
}

getch()
{
	char	buf;

	if (read(ttyfd, &buf, 1) != 1)
		return -1;
	if ((buf & 0x7f) == Ctrl('c')) {
		/*
		 * If you hit ctrl-c, the buffer was left in a
		 * strange state; the next command typed was
		 * mucked up.  Doing all of this is probably
		 * overkill, but it works most of the time.
		 */
		memset(es->cbuf, 0, CMDLEN);
		es->winleft = 0;
		es->cbufsize = 0;
		es->linelen = 0;
		es->cursor = 0;

		memset(undo->cbuf, 0, CMDLEN);
		undo->winleft = 0;
		undo->cbufsize = 0;
		undo->linelen = 0;
		undo->cursor = 0;
		x_mode(FALSE);
		trapsig(SIGINT);
	} else if ((buf & 0x7f) == Ctrl('d'))
		return -1;
	return buf & 0x7f;
}


char **globstr(stuff)
char *stuff;
    {
    char *vecp[2];

    vecp[0] = stuff;
    vecp[1] = NULL;
    return(eval(vecp, DOBLANK|DOGLOB|DOTILDE));
    }
#endif /* VI */
