# .kshrc for OS/2 version of ksh

set -o trackall
set -o ignoreeof

alias a:='cd a:.'
alias b:='cd b:.'
alias c:='cd c:.'
alias d:='cd d:.'
alias e:='cd e:.'
alias f:='cd f:.'
alias g:='cd g:.'

alias h='fc -l'
alias j='jobs'
#alias which='type'
alias back='cd -'
alias cls='print -n "\033[H\033[2J"'

alias dir='cmd /c dir'
alias del='cmd /c del'
alias copy='cmd /c copy'
alias start='cmd /c start'

alias ll='ls -lsAFk'
alias lf='ls -CAFk'
alias cp='cp -p'
alias ls='ls -F'

clock_p () {
PS1='${__[(H=SECONDS/3600%24)==(M=SECONDS/60%60)==(S=SECONDS%60)]-$H:$M:$S}>'
typeset -Z2 H M S; let SECONDS=`date '+(%H*60+%M)*60+%S'`
}

unalias login newgrp

if [ "$KSH_VERSION" = "" ]
then PS1='$PWD>'
     return
fi

set -o emacs
bind ^Q=quote
bind ^I=complete
#bind ^[^[=list-file

#The next four have been preprogrammed
bind ^0H=up-history
bind ^0P=down-history
bind ^0K=backward-char
bind ^0M=forward-char

bind ^0s=backward-word
bind ^0t=forward-word
bind ^0G=beginning-of-line
bind ^0O=end-of-line
bind ^0w=beginning-of-history
bind ^0u=end-of-history
bind ^0S=eot-or-delete


FCEDIT=t2
PS1='[!]$PWD: '
function pushd { 
        if [ $# -eq 0 ]
        then    d=~
                set -A dirstk ${dirstk[*]} $PWD
                cd $d
        else    for d in $* 
                do      if [ -d $d ] && [ -r $d ] && [ -x $d ]
                        then    set -A dirstk ${dirstk[*]} $PWD
                                cd $d
                        else    echo "$d: Cannot change directory"
                                break
                        fi
                done
        fi
        echo ${dirstk[*]} $PWD
        unset d ;
}
 
function popd { 
        if [ ${#dirstk[*]} -gt 0 ]
        then    let n=${#dirstk[*]}-1
                cd ${dirstk[$n]}
                unset dirstk[$n]
                echo ${dirstk[*]} $PWD
        else    echo "popd: Directory stack empty"
        fi
        unset n ; 
}
