#ifndef lint
static char *RCSid = "$Id: vprintf.c,v 1.3 1992/05/12 09:31:03 sjg Exp $";
#endif
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>

#define	BUF	40		/* buffer for int -> string conversion */

int
#ifdef __STDC__
vprintf(const char *fmt, va_list va) {
#else
vprintf(fmt, va) char *fmt; va_list va; {
#endif
	return vfprintf(stdout, fmt, va);
}

int
#ifdef __STDC__
vfprintf(register FILE *f, register const char *fmt, register va_list va) {
#else
vfprintf(f, fmt, va) register FILE *f; register char *fmt; register va_list va; {
#endif
	register int c;
	int pos = 0;			/* todo: implement */

	while ((c = *fmt++))
	    if (c == '%') {
		long n;
		register unsigned long u;
		char buf [BUF+1];
		register char *p = buf + BUF;
		register enum {
			FF_ALT = 0x01, /* #, alternate format */
			FF_SHORT = 0x02, /* h, short arg */
			FF_LONG = 0x04,	/* l, long arg */
			FF_ZERO = 0x08,	/* 0, zero fill */
			FF_LEFT = 0x10,	/* -, left adjust */
			FF_PREC = 0x20,	/* .*, precision */
			FF_NEG = 0x40,	/* signed arg */
			FF_PUTS = 0x80,	/* fputs(p, f) */
			FF_DEFAULT = 0
		} flags = FF_DEFAULT;
		int sign = '-';	/* sign: [ +-] */
		int width = 0, prec = 0; /* width, precision */

		*p = 0;

		/* scan flag characters */
		for (c = *fmt++; ; c = *fmt++) switch (c) {
		  case '0':
			flags |= FF_ZERO;
			break;

		  case '#':		/* alternate format */
			flags |= FF_ALT;
			break;

		  case ' ':		/* blank sign */
			sign = ' ';
			break;
		  case '+':		/* +/- sign */
			sign = '+';
			break;

		  case '-':		/* left just. */
			flags |= FF_LEFT;
			break;

		  default:
			goto Frogs;
		}
	  Frogs:

		/* scan width */
	  	if (c == '*') {		/* width from arg list */
			width = va_arg(va, int);
			c = *fmt++;
		} else
			while ('0' <= c && c <= '9') {
				width = width*10 + (c-'0');
				c = *fmt++;
			}

		if (c == '.') {		/* scan precision */
			flags |= FF_PREC;
			c = *fmt++;
			if (c == '*') {	/* precision from arg list */
				prec = va_arg(va, int);
				c = *fmt++;
			} else
				while ('0' <= c && c <= '9') {
					prec = prec*10 + (c-'0');
					c = *fmt++;
				}
		}

		/* length modifiers */
		if (c == 'h') {
			flags |= FF_SHORT;
			c = *fmt++;
		} else if (c == 'l') {
			flags |= FF_LONG;
			c = *fmt++;
		}

		/* do conversion */
		switch (c) {
		  case '%':		/* %% -> % */
			putc(c, f);
			pos ++;
			break;

		  case 'p':		/* pointer */
			*--p = '}';
			u = (unsigned long) va_arg(va, void*);
			do {
				*--p = "0123456789ABCDEF"[u%16];
				u /= 16;
			} while (u != 0);
			*--p = '{';
			flags |= FF_PUTS;
			break;

		  case 'n':		/* save position */
			*va_arg(va, int*) = pos;
			break;

		  case 'c':		/* character */
			u = (flags&FF_SHORT) ? va_arg(va, unsigned short)
			  : (flags&&FF_LONG) ? va_arg(va, unsigned long)
			  : va_arg(va, unsigned int);
			*--p = u;
			flags |= FF_PUTS;
			break;

		  case 's':		/* string */
			if ((p = va_arg(va, char *)) == NULL)
			  p = "";
			if ((flags&FF_PREC) && strlen(p) > prec) {
				pos += prec;
				while (--prec >= 0)
				{
				  c = *p++;
				  putc(c, f);
				}
				break;
			}
			flags |= FF_PUTS;
			break;

		  case 'i': case 'd': case 'u': /* decimal */
			if (c != 'u') {	/* signed */
				n = (flags&FF_SHORT) ? va_arg(va, short)
				  : (flags&&FF_LONG) ? va_arg(va, long)
				  : va_arg(va, int);
				if (n < 0)
					flags |= FF_NEG;
				u = (n < 0) ? -n : n;
			} else
				u = (flags&FF_SHORT) ? va_arg(va, unsigned short)
				  : (flags&&FF_LONG) ? va_arg(va, unsigned long)
				  : va_arg(va, unsigned int);
			do {
				*--p = '0' + u%10;
				u /= 10;
			} while (u != 0);
			prec -= buf+BUF - p;
			while (--prec >= 0)
				*--p = '0';
			if (flags&FF_NEG)
				*--p = '-';
			else
				if (sign != '-')
					*--p = (sign == '+') ? '+' : ' ';
			flags |= FF_PUTS;
			break;

		  case 'x': case 'X':	/* hex, Hex */
			u = (flags&FF_SHORT) ? va_arg(va, unsigned short)
			  : (flags&&FF_LONG) ? va_arg(va, unsigned long)
			  : va_arg(va, unsigned int);
			do {
				*--p = "0123456789ABCDEF"[u%16];
				u /= 16;
			} while (u != 0);
			prec -= buf+BUF - p;
			while (--prec >= 0)
				*--p = '0';
			if (flags&&FF_ALT)
				*--p = 'x', *--p = '0';
			flags |= FF_PUTS;
			break;

		  case 'o':		/* octal */
			u = (flags&FF_SHORT) ? va_arg(va, unsigned short)
			  : (flags&&FF_LONG) ? va_arg(va, unsigned long)
			  : va_arg(va, unsigned int);
			do {
				*--p = '0' + u%8;
				u /= 8;
			} while (u != 0);
			prec -= buf+BUF - p;
			while (--prec >= 0)
				*--p = '0';
			if (flags&&FF_ALT && *p != '0')
				*--p = '0';
			flags |= FF_PUTS;
			break;

		  default:		/* todo: error */
			putc('%', f);
			putc(c, f);
			pos += 2;
			break;
		}

		/* copy adjusted string "p" to output */
		if (flags&FF_PUTS) {
			int len = strlen(p);
			int pad = width - len;
			if (!(flags&FF_LEFT))
				while (--pad >= 0)
					putc((flags&FF_ZERO) ? '0' : ' ', f);
			while (c = *p++)
				putc(c, f);
			if ((flags&FF_LEFT))
				while (--pad >= 0)
					putc(' ', f);
			pos += (len < width) ? width : len;
		}
	    } else {			/* ordinary character */
		putc(c, f);
		pos ++;
	    }
	return pos;
}

