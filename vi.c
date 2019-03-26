/*
 *	vi command editing
 *	written by John Rochester (initially for nsh)
 *	bludgeoned to fit pdksh by Larry Bouzane, Jeff Sparkes & Eric Gisin
 *
 */
#include "config.h"
#ifdef VI

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: vi.c,v 1.3 1994/05/31 13:34:34 michael Exp michael $";
#endif

#include "sh.h"
#include <ctype.h>
#include "ksh_stat.h"		/* completion */
#include "expand.h"
#include "edit.h"

#define CMDLEN		1024
#define Ctrl(c)		(c&0x1f)
#define	iswordch(c)	(letnum(c))

typedef struct glob Glob;

static void 	vi_reset ARGS((char *buf, size_t len));
static int	nextstate ARGS((int ch));
static int	vi_insert ARGS((int ch));
static int	vi_cmd ARGS((int argcnt, char *cmd));
static int	domove ARGS((int argcnt, char *cmd, int sub));
static int	redo_insert ARGS((int count));
static void	yank_range ARGS((int a, int b));
static int	bracktype ARGS((int ch));
static void	save_cbuf ARGS((void));
static void	restore_cbuf ARGS((void));
static void	edit_reset ARGS((char *buf, size_t len));
static int	putbuf ARGS((char *buf, int len, int repl));
static void	del_range ARGS((int a, int b));
static int	findch ARGS((int ch, int cnt, int forw, int incl));
static int	forwword ARGS((int argcnt));
static int	backword ARGS((int argcnt));
static int	endword ARGS((int argcnt));
static int	Forwword ARGS((int argcnt));
static int	Backword ARGS((int argcnt));
static int	Endword ARGS((int argcnt));
static int	grabhist ARGS((int save, int n));
static int	grabsearch ARGS((int save, int start, int fwd, char *pat));
static void	redraw_line ARGS((void));
static void	refresh ARGS((int leftside));
static int	outofwin ARGS((void));
static void	rewindow ARGS((void));
static int	newcol ARGS((int ch, int col));
static void	display ARGS((char *wb1, char *wb2, int leftside));
static void	ed_mov_opt ARGS((int col, char *wb));
static char	**globstr ARGS((char *cp));
static int 	glob_word ARGS((Glob *g, int command));
static int	expand_word ARGS((int command));
static int	complete_word ARGS((int command));
static int	print_expansions ARGS((int command));
static void 	x_vi_zotc ARGS((int c));

#define C_	0x1
#define M_	0x2
#define E_	0x4
#define X_	0x8
#define U_	0x10
#define B_	0x20
#define O_	0x40
#define S_	0x80

#define isbad(c)	(classify[(c)&0x7f]&B_)
#define iscmd(c)	(classify[(c)&0x7f]&(M_|E_|C_|U_))
#define ismove(c)	(classify[(c)&0x7f]&M_)
#define isextend(c)	(classify[(c)&0x7f]&E_)
#define islong(c)	(classify[(c)&0x7f]&X_)
#define ismeta(c)	(classify[(c)&0x7f]&O_)
#define isundoable(c)	(!(classify[(c)&0x7f]&U_))
#define issrch(c)	(classify[(c)&0x7f]&S_)

char	classify[128] = {
	/*	0	1	2	3	4	5	6	7  */
/*  0- */	B_,	0,	0,	0,	0,	C_|U_,	O_,	0,
/*  1- */	C_|M_,	0,	O_,	0,	C_|U_,	O_,	O_|C_,	0,
/*  2- */	O_|C_,	0,	C_|U_,	0,	0,	0,	0,	0,
/*  3- */	0,	0,	O_,	C_,	0,	0,	0,	0,
/*  4- */	M_,	0,	0,	C_,	M_,	M_,	0,	0,
/*  5- */	0,	0,	C_,	C_,	M_,	C_,	0,	C_|S_,
/*  6- */	M_,	0,	0,	0,	0,	0,	0,	0,
/*  7- */	0,	0,	0,	M_,	0,	C_,	0,	C_|S_,
/* 10- */	0,	C_,	M_,	C_,	C_,	M_,	M_|X_,	C_,
/* 11- */	0,	C_,	0,	0,	0,	0,	C_,	0,
/* 12- */	C_,	0,	C_,	C_,	M_|X_,	0,	0,	M_,
/* 13- */	C_,	C_,	0,	0,	C_,	0,	M_,	C_,
/* 14- */	0,	C_,	M_,	E_,	E_,	M_,	M_|X_,	0,
/* 15- */	M_,	C_,	C_,	C_,	M_,	0,	C_,	0,
/* 16- */	C_,	0,	X_,	C_,	M_|X_,	C_|U_,	0,	M_,
/* 17- */	C_,	E_,	0,	0,	M_,	0,	C_,	0
};

#define MAXVICMD	3
#define SRCHLEN		40

#define INSERT		1
#define REPLACE		2

#define VNORMAL		0		/* command, insert or replace mode */
#define VARG1		1		/* digit prefix (first, eg, 5l) */
#define VEXTCMD		2		/* cmd + movement (eg, cl) */
#define VARG2		3		/* digit prefix (second, eg, 2c3l) */
#define VXCH		4		/* f, F, t, T */
#define VFAIL		5		/* bad command */
#define VCMD		6		/* single char command (eg, X) */
#define VREDO		7		/* . */
#define VLIT		8		/* ^V */
#define VSEARCH		9		/* /, ? */

struct edstate {
	int	winleft;
	char	*cbuf;
	int	cbufsize;
	int	linelen;
	int	cursor;
};

struct glob {
	char 	**result;
	int	start, end;
	int	addspace;
};

static char		undocbuf[CMDLEN];

static struct edstate 	*save_edstate ARGS((struct edstate *old));
static void		restore_edstate ARGS((struct edstate *old, struct edstate *new));
static void 		free_edstate ARGS((struct edstate *old));

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

enum expand_mode { NONE, EXPAND, COMPLETE, PRINT };
static enum expand_mode expanded = NONE;/* last input was expanded */

int
vi_hook(ch)
	int		ch;
{
	static char	curcmd[MAXVICMD];
	static char	locpat[SRCHLEN];
	static int	cmdlen;
	static int	argc1, argc2;

	switch (state) {

	case VNORMAL:
		if (insert != 0) {
			if (ch == Ctrl('v')) {
				state = VLIT;
				ch = '^';
			}
			switch (vi_insert(ch)) {
			case -1:
				x_putc(BEL);
				state = VNORMAL;
				break;
			case 0:
				if (state == VLIT) {
					es->cursor--;
					refresh(0);
				} else
					refresh(insert != 0);
				break;
			case 1:
				return 1;
			}
		} else {
			if (ch == '\r' || ch == '\n')
				return 1;
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
			x_putc(BEL);
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
		if (ch == '\r' || ch == '\n' /*|| ch == Ctrl('[')*/ ) {
			locpat[srchlen] = '\0';
			(void) strcpy(srchpat, locpat);
			state = VCMD;
		} else if (ch == edchars.erase || ch == Ctrl('h')) {
			if (srchlen != 0) {
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
			restore_cbuf();
			state = VNORMAL;
			refresh(0);
		} else if (ch == edchars.kill) {
			srchlen = 0;
			es->linelen = 1;
			es->cursor = 1;
			refresh(0);
			return 0;
		} else if (ch == edchars.werase) {
			int i;
			int n = srchlen;

			while (n > 0 && isspace(locpat[n - 1]))
				n--;
			while (n > 0 && !isspace(locpat[n - 1]))
				n--;
			for (i = srchlen; --i >= n; ) {
				if (locpat[i] < ' ' || locpat[i] == 0x7f)
					es->linelen--;
				es->linelen--;
			}
			srchlen = n;
			es->cursor = es->linelen;
			refresh(0);
			return 0;
		} else {
			if (srchlen == SRCHLEN - 1)
				x_putc(BEL);
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
		}
		break;
	}

	switch (state) {
	case VCMD:
		state = VNORMAL;
		switch (vi_cmd(argc1, curcmd)) {
		case -1:
			x_putc(BEL);
			break;
		case 0:
			if (insert != 0)
				inslen = 0;
			refresh(insert != 0);
			break;
		case 1:
			refresh(0);
			return 1;
		}
		break;

	case VREDO:
		state = VNORMAL;
		if (argc1 != 0)
			lastac = argc1;
		switch (vi_cmd(lastac, lastcmd) != 0) {
		case -1:
			x_putc(BEL);
			refresh(0);
			break;
		case 0:
			if (insert != 0) {
				if (lastcmd[0] == 's' || lastcmd[0] == 'c' ||
						lastcmd[0] == 'C') {
					if (redo_insert(1) != 0)
						x_putc(BEL);
				} else {
					if (redo_insert(lastac) != 0)
						x_putc(BEL);
				}
			}
			refresh(0);
			break;
		case 1:
			refresh(0);
			return 1;
		}
		break;

	case VFAIL:
		state = VNORMAL;
		x_putc(BEL);
		break;
	}
	return 0;
}

static void
vi_reset(buf, len)
	char	*buf;
	size_t	len;
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

static int
nextstate(ch)
	int	ch;
{
	if (isextend(ch))
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

	if (ch == edchars.erase || ch == Ctrl('h')) {
		if (insert == REPLACE) {
			if (es->cursor == undo->cursor) {
				x_putc(BEL);
				return 0;
			}
			if (inslen > 0)
				inslen--;
			es->cursor--;
			if (es->cursor >= undo->linelen)
				es->linelen--;
			else
				es->cbuf[es->cursor] = undo->cbuf[es->cursor];
		} else {
			if (es->cursor == 0) {
				/* x_putc(BEL); no annoying bell here */
				return 0;
			}
			if (inslen > 0)
				inslen--;
			es->cursor--;
			es->linelen--;
			memmove(&es->cbuf[es->cursor], &es->cbuf[es->cursor+1],
					es->linelen - es->cursor + 1);
		}
		expanded = NONE;
		return 0;
	}
	if (ch == edchars.kill) {
		if (es->cursor != 0) {
			inslen = 0;
			memmove(es->cbuf, &es->cbuf[es->cursor],
						es->linelen - es->cursor);
			es->linelen -= es->cursor;
			es->cursor = 0;
		}
		expanded = NONE;
		return 0;
	}
	if (ch == edchars.werase) {
		if (es->cursor != 0) {
			tcursor = Backword(1);
			memmove(&es->cbuf[tcursor], &es->cbuf[es->cursor],
						es->linelen - es->cursor);
			es->linelen -= es->cursor - tcursor;
			if (inslen < es->cursor - tcursor)
				inslen = 0;
			else
				inslen -= es->cursor - tcursor;
			es->cursor = tcursor;
		}
		expanded = NONE;
		return 0;
	}
	switch (ch) {

	case '\0':
		return -1;

	case '\r':
	case '\n':
		return 1;

	case Ctrl('['):
		expanded = NONE;
		if (lastcmd[0] == 's' || lastcmd[0] == 'c' ||
				lastcmd[0] == 'C')
			return redo_insert(0);
		else
			return redo_insert(lastac - 1);

	/* { Begin nonstandard vi commands */
	case Ctrl('x'):
		expand_word(0);
		break;

	case Ctrl('f'):
		complete_word(0);
		break;

	case Ctrl('e'):
		print_expansions(0);
		break;

	case Ctrl('i'):
		if (Flag(FVITABCOMPLETE)) {
			complete_word(0);
			break;
		}
		/* FALLTHROUGH */
	/* End nonstandard vi commands } */

	default:
		if (es->linelen == es->cbufsize - 1)
			return -1;
		ibuf[inslen++] = ch;
		if (insert == INSERT) {
			memmove(&es->cbuf[es->cursor+1], &es->cbuf[es->cursor],
					es->linelen - es->cursor);
			es->linelen++;
		}
		es->cbuf[es->cursor++] = ch;
		if (insert == REPLACE && es->cursor > es->linelen)
			es->linelen++;
		expanded = NONE;
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
		else if (*cmd != '_' && *cmd != '|')
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
			memmove(undo->cbuf, es->cbuf, es->linelen);
			undo->linelen = es->linelen;
			undo->cursor = es->cursor;
			lastac = argcnt;
			memmove(lastcmd, cmd, MAXVICMD);
		}
		switch (*cmd) {

		case Ctrl('l'):
		case Ctrl('r'):
			redraw_line();
			break;

		case 'a':
			modified = 1; hnum = hlast;
			if (es->linelen != 0)
				es->cursor++;
			insert = INSERT;
			break;

		case 'A':
			modified = 1; hnum = hlast;
			del_range(0, 0);
			es->cursor = es->linelen;
			insert = INSERT;
			break;

		case 'S':
			del_range(0, es->linelen);
			es->cursor = 0;
			modified = 1; hnum = hlast;
			insert = INSERT;
			break;

		case 'Y':
			cmd = "y$";
			/* ahhhhhh... */
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
				modified = 1; hnum = hlast;
				insert = INSERT;
			}
			break;

		case 'p':
			modified = 1; hnum = hlast;
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
			modified = 1; hnum = hlast;
			while (putbuf(ybuf, yanklen, 0) == 0 && --argcnt > 0)
				;
			if (es->cursor != 0)
				es->cursor--;
			if (argcnt != 0)
				return -1;
			break;

		case 'C':
			modified = 1; hnum = hlast;
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
			modified = 1; hnum = hlast;
			insert = INSERT;
			break;

		case 'I':
			modified = 1; hnum = hlast;
			es->cursor = 0;
			insert = INSERT;
			break;

		case 'j':
		case '+':
		case Ctrl('n'):
			if (grabhist(modified, hnum + argcnt) < 0)
				return -1;
			else {
				modified = 0;
				hnum += argcnt;
			}
			break;

		case 'k':
		case '-':
		case Ctrl('p'):
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
			modified = 1; hnum = hlast;
			if (cmd[1] == 0)
				x_putc(BEL);
			else
				es->cbuf[es->cursor] = cmd[1];
			break;

		case 'R':
			modified = 1; hnum = hlast;
			insert = REPLACE;
			break;

		case 's':
			if (es->linelen == 0)
				return -1;
			modified = 1; hnum = hlast;
			if (es->cursor + argcnt > es->linelen)
				argcnt = es->linelen - es->cursor;
			del_range(es->cursor, es->cursor + argcnt);
			insert = INSERT;
			break;

		case 'x':
			if (es->linelen == 0)
				return -1;
			modified = 1; hnum = hlast;
			if (es->cursor + argcnt > es->linelen)
				argcnt = es->linelen - es->cursor;
			yank_range(es->cursor, es->cursor + argcnt);
			del_range(es->cursor, es->cursor + argcnt);
			break;

		case 'X':
			if (es->cursor > 0) {
				modified = 1; hnum = hlast;
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
			if (hnum == hlast)
				hnum = -1;
			/* ahhh */
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

			if (histnum(-1) < 0)
				return -1;
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
			modified = 1; hnum = hlast;
			if (es->cursor != es->linelen)
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
				modified = 1; hnum = hlast;
				*p = toupper(*p);
			} else if (isupper(*p)) {
				modified = 1; hnum = hlast;
				*p = tolower(*p);
			}
			if (es->cursor < es->linelen - 1)
				es->cursor++;
			}
			break;

		case '#':
			es->cursor = 0;
			if (es->linelen > 0 && putbuf("#", 1, 0) != 0)
				return -1;
			return 1;

		case '=': 			/* at&t ksh */
		case Ctrl('e'):			/* Nonstandard vi/ksh */
			print_expansions(1);
			break;

		case Ctrl('['):			/* at&t ksh */
		case '\\':			/* at&t ksh */
		case Ctrl('f'):			/* Nonstandard vi/ksh */
			complete_word(0);
			break;


		case '*':			/* at&t ksh */
		case Ctrl('x'):			/* Nonstandard vi/ksh */
			expand_word(1);
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
	int	bcount, UNINITIALIZED(i), t;
	int	UNINITIALIZED(ncursor);

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
		if (sub && ncursor < es->linelen)
			ncursor++;
		break;

	case 'E':
		if (!sub && es->cursor + 1 >= es->linelen)
			return -1;
		ncursor = Endword(argcnt);
		if (sub && ncursor < es->linelen)
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
	case Ctrl('h'):
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
			if (ncursor > es->linelen)
				ncursor = es->linelen;
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

	case '|':
		ncursor = argcnt;
		if (ncursor > es->linelen)
			ncursor = es->linelen;
		if (ncursor)
			ncursor--;
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

static void
yank_range(a, b)
	int	a, b;
{
	yanklen = b - a;
	if (yanklen != 0)
		memmove(ybuf, &es->cbuf[a], yanklen);
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
static char	*wbuf[2];		/* window buffers */
static int	wbuf_len;		/* length of window buffers (x_cols-3)*/
static int	win;			/* window buffer in use */
static char	morec;			/* more character at right of window */
static int	lastref;		/* argument to last refresh() */
static char	holdbuf[CMDLEN];	/* place to hold last edit buffer */
static int	holdlen;		/* length of holdbuf */

static void
save_cbuf()
{
	memmove(holdbuf, es->cbuf, es->linelen);
	holdlen = es->linelen;
	holdbuf[holdlen] = '\0';
}

static void
restore_cbuf()
{
	es->cursor = 0;
	es->linelen = holdlen;
	memmove(es->cbuf, holdbuf, holdlen);
}

/* return a new edstate */
static struct edstate *
save_edstate(old)
	struct edstate *old;
{
	struct edstate *new;

	new = (struct edstate *)alloc(sizeof(struct edstate), APERM);
	new->cbuf = alloc(old->cbufsize, APERM);
	new->cbufsize = old->cbufsize;
	strcpy(new->cbuf, old->cbuf);
	new->linelen = old->linelen;
	new->cursor = old->cursor;
	new->winleft = old->winleft;
	return new;
}

static void
restore_edstate(new, old)
	struct edstate *old, *new;
{
	strncpy(new->cbuf, old->cbuf, old->linelen);
	new->linelen = old->linelen;
	new->cursor = old->cursor;
	new->winleft = old->winleft;
	free_edstate (old);
}

static void
free_edstate(old)
	struct edstate *old;
{
	afree(old->cbuf, APERM);
	afree((char *)old, APERM);
}
	
	

static void
edit_reset(buf, len)
	char	*buf;
	size_t	len;
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
	if (!wbuf_len || wbuf_len != x_cols - 3) {
		wbuf_len = x_cols - 3;
		wbuf[0] = aresize(wbuf[0], wbuf_len, APERM);
		wbuf[1] = aresize(wbuf[1], wbuf_len, APERM);
	}
	(void) memset(wbuf[0], ' ', wbuf_len);
	(void) memset(wbuf[1], ' ', wbuf_len);
	winwidth = x_cols - pwidth - 3;
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
		memmove(&es->cbuf[es->cursor + len], &es->cbuf[es->cursor],
			es->linelen - es->cursor);
		es->linelen += len;
	}
	memmove(&es->cbuf[es->cursor], buf, len);
	es->cursor += len;
	return 0;
}

static void
del_range(a, b)
	int	a, b;
{
	if (es->linelen != b)
		memmove(&es->cbuf[a], &es->cbuf[b], es->linelen - b);
	es->linelen -= b - a;
}

static int
findch(ch, cnt, forw, incl)
	int	ch;
	int	cnt;
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

static int
forwword(argcnt)
	int	argcnt;
{
	int	ncursor;

	ncursor = es->cursor;
	while (ncursor < es->linelen && argcnt--) {
		if (iswordch(es->cbuf[ncursor]))
			while (iswordch(es->cbuf[ncursor]) &&
					ncursor < es->linelen)
				ncursor++;
		else if (!isspace(es->cbuf[ncursor]))
			while (!iswordch(es->cbuf[ncursor]) &&
					!isspace(es->cbuf[ncursor]) &&
					ncursor < es->linelen)
				ncursor++;
		while (isspace(es->cbuf[ncursor]) && ncursor < es->linelen)
			ncursor++;
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
			if (iswordch(es->cbuf[ncursor]))
				while (--ncursor >= 0 &&
				   iswordch(es->cbuf[ncursor]))
					;
			else
				while (--ncursor >= 0 &&
				   !iswordch(es->cbuf[ncursor]) &&
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
			if (iswordch(es->cbuf[ncursor]))
				while (++ncursor < es->linelen &&
					  iswordch(es->cbuf[ncursor]))
					;
			else
				while (++ncursor < es->linelen &&
				   !iswordch(es->cbuf[ncursor]) &&
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
		while (!isspace(es->cbuf[ncursor]) && ncursor < es->linelen)
			ncursor++;
		while (isspace(es->cbuf[ncursor]) && ncursor < es->linelen)
			ncursor++;
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
	memmove(es->cbuf, hptr, es->linelen);
	es->cursor = 0;
	return 0;
}

static int
grabsearch(save, start, fwd, pat)
	int	save, start, fwd;
	char	*pat;
{
	char	*hptr;
	int	hist;

	if ((start == 0 && fwd == 0) || (start >= hlast-1 && fwd == 1))
		return -1;
	if (fwd)
		start++;
	else
		start--;
	if ((hist = findhist(start, fwd, pat)) < 0) {
		/* if (start != 0 && fwd && match(holdbuf, pat) >= 0) { */
		if (start != 0 && fwd && strcmp(holdbuf, pat) >= 0) {
			restore_cbuf();
			return 0;
		} else
			return -1;
	}
	if (save)
		save_cbuf();
	histnum(hist);
	hptr = *histpos();
	es->linelen = strlen(hptr);
	memmove(es->cbuf, hptr, es->linelen);
	es->cursor = 0;
	return hist;
}

static void
redraw_line()
{
	(void) memset(wbuf[win], ' ', wbuf_len);
	x_putc('\r');
	x_putc('\n');
	pprompt(prompt);
	cur_col = pwidth;
	morec = ' ';
}

static void
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
	if (col >= winwidth)
		return 1;
	return 0;
}

static void
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

static void
display(wb1, wb2, leftside)
	char	*wb1, *wb2;
	int	leftside;
{
	char	*twb1, *twb2, mc;
	int	cur, col, cnt;
	int	UNINITIALIZED(ncol);
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
	if (cur_col != ncol)
		ed_mov_opt(ncol, wb1);
}

static void
ed_mov_opt(col, wb)
	int	col;
	char	*wb;
{
	if (col < cur_col) {
		if (col + 1 < cur_col - col) {
			x_putc('\r');
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
	char	*buf;
	size_t	len;
{
	int	c;
	
	vi_reset(buf, len > CMDLEN ? CMDLEN : len);
	x_flush();
	while (1) {
		if ((c = x_getc()) == -1)
			break;
		if (state != VLIT) {
			if (c == edchars.intr || c == edchars.quit) {
				/* pretend we got an interrupt */
				x_vi_zotc(c);
				x_flush();
				shf_flush(shl_out);
				trapsig(c == edchars.intr ? SIGINT : SIGQUIT);
				runtraps(TRUE);
				continue;
			} else if (c == Ctrl('d')) {
				if (es->linelen == 0) {
					x_putc('^'); x_putc('D');
					c = -1;
					break;
				}
				continue;
			}
		}
		if (vi_hook(c))
			break;
		x_flush();
	}

	x_putc('\r'); x_putc('\n'); x_flush();

	if (c == -1)
		return -1;

	if (es->cbuf != buf)
		memmove(buf, es->cbuf, es->linelen);

	buf[es->linelen++] = '\n';

	return es->linelen;
}

/*
 * glob string.  Useful for the command line editors.
 * (mostly just a copy of eval())
 */
static char **
globstr(cp)
	char		*cp;
{
	XPtrV		w;
	struct source	*s, *sold;

	sold = source;
	s = pushs(SWSTR);
	s->str = (char *) cp;
	source = s;
	if (yylex(ONEWORD) != LWORD) {
		source = sold;
		shellf("eval:substitute error\n");
		return (char **) 0;
	}
	source = sold;
	XPinit(w, 10);
	expand(yylval.cp, &w, (DOGLOB|DOTILDE));
	XPput(w, NULL);
	return (char **) XPclose(w);

#ifdef notdef
	XPtrV w;

	XPinit(w, 10);
	XPput(w, NULL);		/* space for shell name */
	expand(cp, &w, (DOGLOB|DOTILDE));
	XPput(w, NULL);
	return (char **) XPclose(w) + 1;
#endif	/* notdef */
}

static int
glob_word(g, command)
	Glob	*g;
	int	command;
{
	int		rval;
	int		star;
	int		start, end;
	char		*toglob = undo->cbuf;

	if (!command && es->linelen > 0)
		es->cursor--;
	/*
	 *	expansion normally maps a*b to `^a.*b.*'; if the cursor
	 *  is just past the end of the pattern, then map a*b to `^a.*b$'.
	 */
	if (isspace(es->cbuf[es->cursor])) {
		if (es->cursor == 0 || isspace(es->cbuf[es->cursor - 1])) {
			if (!command && es->linelen > 0)
				es->cursor++;
			x_putc(BEL);
			return -1;
		}
		start = es->cursor - 1;
		star = 0;
	} else {
		start = es->cursor;
		star = 1;
	}
	while (start > -1 && !isspace(es->cbuf[start]))
		start--;
	start++;
	end = es->cursor;
	while (end < es->linelen && !isspace(es->cbuf[end]))
		end++;
	if (start == end) {
		if (!command && es->linelen > 0)
			es->cursor++;
		return -1;
	}

	/* use undo buffer to build word up in */
	memmove(toglob, &es->cbuf[start], end-start);
	if (star && toglob[end-start-1] != '*') {
		toglob[end-start] = '*';
		toglob[end-start+1] = '\0';
	} else
		toglob[end-start] = '\0';

	g->start = start;
	g->end = end;
	g->result = globstr(toglob);
	g->addspace = 0;
	rval = 0;

	if (strcmp(g->result[0], toglob) == 0 && g->result[1] == (char *) 0) {
		struct stat	statb;

		/* If no star was appended, we need an additional check to
		 * see if this really is an error.  E.g. toglob == "abc"
		 * when the file "abc" exists is not an error; if
		 * "abc" does exist it is an error.
		 */
		if (star || stat(g->result[0], &statb) < 0) {
			if (!command && es->linelen > 0)
				es->cursor++;
			x_putc(BEL);
			rval = -1;
		}
	}

	if (rval == 0 && isspace(es->cbuf[es->cursor]))
		g->addspace = 1;

	/* restore undo buffer that we used temporarily */
	memmove(toglob, es->cbuf, es->linelen);
	return 0;
}

static int
expand_word(command)
	int command;
{
	int	rval = 0;
	Glob	g;
	static 	struct edstate *buf = 0;

	/* Undo previous expansion */
	if (command == 0 && expanded == EXPAND && buf) {
		restore_edstate(es, buf);
		if (es->cursor != es->linelen)
			es->cursor++;
		buf = 0;
		expanded = NONE;
		return 0;
	}
	if (glob_word(&g, command) < 0)
		return -1;
	buf = save_edstate(es);
	expanded = EXPAND;
	del_range(g.start, g.end);
	es->cursor = g.start;
	if (g.result == (char **)0)
		rval = -1;
	else while (1) {
		if (putbuf(*g.result, (int) strlen(*g.result), 0) != 0) {
			rval = -1;
			break;
		}
		if (*++g.result == (char *) 0)
			break;
		if (putbuf(" ", 1, 0) != 0) {
			rval = -1;
			break;
		}
	}
	if (g.addspace && rval == 0)
		rval = putbuf(" ", 1, 0);
	modified = 1; hnum = hlast;
	insert = INSERT;
	refresh(0);
	return rval;
}

static int
complete_word(command)
	int command;
{
	int	rval = 0;
	Glob	g;
	int	i, escape = 0, len;
	struct	stat s;
	static 	struct edstate *buf = 0;

	/* Undo previous completion */
	if (command == 0 && expanded == COMPLETE) {
		print_expansions(0);
		expanded = PRINT;
		return 0;
	}
	if (command == 0 && expanded == PRINT && buf) {
		restore_edstate(es, buf);
		if (es->cursor != es->linelen)
			es->cursor++;
		buf = 0;
		expanded = NONE;
		return 0;
	}
	if (glob_word(&g, command) < 0)
		return -1;
	/* Check for globbing in arg string */
	for (i=g.start; i<g.end; i++) {
		switch (es->cbuf[i]) {
		case '\\':
			escape = 1;
			break;
		case '*':
		case '?':
		case '[':
			if (!escape) {
				x_putc(BEL);
				return -1;
			}
			break;
		default:
			escape = 0;
		}
	}

	buf = save_edstate(es);
	expanded = COMPLETE;
	del_range(g.start, g.end);
	es->cursor = g.start;
	if (g.result == (char **)0)
		rval = -1;
	else {
		/* find the shortest match */
		char	*save = g.result[0], *p;
		len = strlen(g.result[0]);
		if (g.result[1] != 0) 
			x_putc(BEL);
		for (i=1; (p = g.result[i]); i++) {
			int j;

			for (j=0; j<len; j++)
				if (p[j] != save[j]) {
					len = j;
					break;
				}
		}
		if (putbuf(g.result[0], len, 0) != 0) 
			rval = -1;
	}
	/* Check to see if it's a directory. */
	if (g.result[1] == 0) {
		if (stat(g.result[0], &s) < 0)
			rval = -1;
		else {
			if (S_ISDIR(s.st_mode))
				putbuf("/", 1, 0);
			else
				putbuf(" ", 1, 0);
		}
	}
	modified = 1; hnum = hlast;
	insert = INSERT;
	refresh(0);
	return rval;
}

static int
print_expansions(command)
	int	command;
{
	Glob 	g;
	char	**p;
	int	i;

	if (glob_word(&g, command) < 0)
		return -1;
	if ((p = g.result) == (char **)0)
		return -1;
	x_putc('\r');
	x_putc('\n');
	for (i = 0; i < cur_col; i++)
		x_putc(' ');
	while (*p && **p) {
		char 	*q;
		if ((q = strrchr(*p, '/')) == 0)
			x_puts(*p);
		else
			x_puts(++q);
		x_putc(' ');
		p++;
	}
	if (!command && es->cursor != es->linelen)
		es->cursor++;
	redraw_line();
	return 0;
}

/* Same as x_zotc(emacs.c), but no tab wierdness */
static void
x_vi_zotc(c)
	int c;
{
	c &= 0x7f;
	if (c < ' ' || c == 0x7f) {
		x_putc('^');
		c ^= '@';
	}
	x_putc(c);
}

#endif	/* VI */
