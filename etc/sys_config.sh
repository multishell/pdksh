:
# NAME:
#	sys_config.sh - set system specific variables
#
# SYNOPSIS:
#	. /etc/sys_config.sh
#
# DESCRIPTION:
#	Source this script into shell scripts that want to handle
#	various system types.
#	You may well want to edit this on a particular system replacing 
#	`uname -s` etc with the result.  So that the facility will work 
#	even when in single user mode and uname et al are not available.
#
# SEE ALSO:
#	/etc/profile
#
# AMENDED:
#	91/11/05 22:09:08 (rook)
#
# RELEASED:
#	91/11/05 22:09:09 v1.3
#
# SCCSID:
#	@(#)sys_config.sh 1.3 91/11/05 22:09:08 (rook)
#
#	@(#)Copyright (c) 1991 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#

# determin machine type
if [ -f /usr/bin/arch ]; then
	ARCH=`arch`
elif [ -f /usr/bin/uname -o -f /bin/uname ]; then
	ARCH=`uname -m`
fi
#
case "$ARCH" in
sun386)	uname=/usr/5bin/uname
	OS=SunOS
	;;
*)	uname=uname;;
esac

# set the operating system type
# you can't use `uname -s` with SCO UNIX
# it returns the same string as `uname -n`
# so set it manually
# OS=SCO-UNIX
# The eval below is a workaround for a bug in the PD ksh.
OS=${OS:-`eval $uname -s`}
HOSTNAME=${HOSTNAME:-`eval $uname -n`}

# set which ever is required to not produce a linefeed 
# in an echo(1)
case $OS in
SunOS)	C="\c"; N="";
	;;
*)	C="\c"; N=""
	;;
esac
export OS ARCH HOSTNAME C N uname
