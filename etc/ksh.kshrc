:
# NAME:
#	ksh.kshrc - global initialization for ksh 
#
# DESCRIPTION:
#	Each invocation of /bin/ksh processes the file pointed
#	to by $ENV (usually $HOME/.kshrc).
#	This file is intended as a global .kshrc file for the
#	Korn shell.  A user's $HOME/.kshrc file simply requires
#	the line:
#		. /etc/ksh.kshrc
#	at or near the start to pick up the defaults in this
#	file which can then be overridden as desired.
#
# SEE ALSO:
#	$HOME/.kshrc
#
# RCSid:
#	$Id: ksh.kshrc,v 1.2 1992/04/27 07:09:28 sjg Exp $
#	@(#)Copyright (c) 1991 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 

case "$-" in
*i*)	# we are interactive
	# we may have su'ed so reset these
	# NOTE: SCO-UNIX doesn't have whoami,
	#	install whoami.sh
	USER=`whoami`
	PROMPT="<$USER@$HOSTNAME:!>$ "
	PPROMPT='<$USER@$HOSTNAME:$PWD:!>$ '
	PS1=$PPROMPT
	# $TTY is the tty we logged in on,
	# $tty is that which we are in now (might by pty)
	tty=`tty`
	tty=`basename $tty`

	set -o ${FCEDIT:-$EDITOR}

	# the PD ksh is not 100% compatible
	case "$KSH_VERSION" in
	*PD*)	# PD ksh
	        bind ^?=delete-char-backward
        	bind ^[^?=delete-word-backward
		;;
	*)	# real ksh ?
		;;
	esac
	case "$TERM" in
	sun*)
		if [ "$tty" != console ]; then
			ILS='\033]L'; ILE='\033\\'
			WLS='\033]l'; WLE='\033\\'
		fi
		;;
	xterm*)
		ILS='\033]1;'; ILE='\007'
		WLS='\033]2;xterm: '; WLE='\007'
		;;
	*)	;;
	esac
	# do we want window decorations?
	if [ "$ILS" ]; then
		wftp () { ilabel "ftp $*"; "ftp" $*; ilabel "$USER@$HOSTNAME"; }
		wcd () { "cd" $*; eval stripe; }
		ilabel () { print -n "${ILS}$*${ILE}"; }
		label () { print -n "${WLS}$*${WLE}"; }
		alias stripe='label $USER @ $HOSTNAME \($tty\) - $PWD'
		alias cd=wcd
		alias ftp=wftp
		eval stripe
		eval ilabel "$USER@$HOSTNAME"
		PS1=$PROMPT
	fi
	alias ls='ls -CF'
	alias h='fc -l | more'
	alias quit=exit
	alias cls=clear
	alias logout=exit
	alias bye=exit


# add your favourite aliases here
;;
*)	# non-interactive
;;
esac
# commands for both interactive and non-interactive shells
