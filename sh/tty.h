/*
	tty.h -- centralized definitions for a variety of terminal interfaces

	created by DPK, Oct. 1986

	last edit:	30-Jul-1987	D A Gwyn
*/

#if _BSD_SYSV			/* BRL UNIX System V emulation */
#include <termio.h>	/* includes <sys/_ioctl.h> */
#ifndef NTTYDISC
#define	TIOCGETD	_IOR( 't', 0, int )
#define	TIOCSETD	_IOW( 't', 1, int )
#define	NTTYDISC	2
#endif
#ifndef TIOCSTI
#define	TIOCSTI		_IOW( 't', 114, char )
#endif
#ifndef TIOCSPGRP
#define	TIOCSPGRP	_IOW( 't', 118, int )
#endif
#else	/* !_BSD_SYSV */
#if _BSD
#ifdef _MINIX
#include <sgtty.h>
#define TIOCSETN	TIOCSETP
#else
#include <sys/ioctl.h>
#endif
#else
#ifdef	mips
#include <termios.h>
#else
#include <termio.h>
#endif
#endif
#endif	/* _BSD_SYSV */
