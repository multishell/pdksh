#!/bin/sh

#
# Update the date in the version file (version.c).  If the existing
# date is todays date, a .number is apprended (or incremented) to
# make them distinct.
#

# pattern that matches the date in the version string in version.c
# \1 is preamble, \2 is the date, \3 is postamble (use ? pattern delimiters).
DATEPAT='\(.*KSH_VERSION=.* \)\([0-9]*/[0-9]*/[.0-9]*\)\(.*\)'

vfile=version.c
bfile=version.c.bak
tfile=version.c.new

odatev=`sed -n "s?$DATEPAT?\2?p" < $vfile`
odate=`echo "$odatev" | sed 's?\..*??'`
ov=`echo "$odatev" | sed 's?[^.]*\.*??'`

date=`date '+%y/%m/%d' 2> /dev/null`
case "$date" in
[0-9]*/[0-9]*/[0-9]*) ;;
*)
	# old system - try to compensate...
	date=`date | awk 'BEGIN {
		months["Jan"] = 1; months["Feb"] = 2; months["Mar"] = 3;
		months["Apr"] = 4; months["May"] = 5; months["Jun"] = 6;
		months["Jul"] = 7; months["Aug"] = 8; months["Sep"] = 9;
		months["Oct"] = 10; months["Nov"] = 11; months["Dec"] = 12;
	    } {
		if (months[$2])
			mon = sprintf("%02d", months[$2]);
		else
			mon = $2;
		printf "%02d/%s/%02d\n", $6 % 100, mon, $3;
	    }'`
esac

if test x"$odate" = x"$date"; then
	v=".$ov"
	if test -z "$ov" ; then
		v=1
	else
		v=`expr $ov + 1`
	fi
	date="$date.$v"
fi

# try to save permissions/ownership/group
cp -p $vfile $tfile 2> /dev/null
if sed "s?$DATEPAT?\1$date\3?" < $vfile > $tfile; then
	rm -f $bfile
	ln $vfile $bfile || exit 1
	mv $tfile $vfile
	exit $?
else
	echo "$0: error creating new $vfile" 1>&2
	exit 1
fi
