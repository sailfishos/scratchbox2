# sb2.bash - bash completion for scratchbox2
#
# Copyright (C) 2013 Jolla Ltd.
# Contact: Juha Kallioinen <juha.kallioinen@jollamobile.com>
#
# Licensed under GPL version 2

_sb2()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="-h -m -L -t"

    case "${prev}" in
	-t)
	    local targets=$(sb2-config -f)
	    COMPREPLY=( $(compgen -W "${targets}" -- ${cur}) )
	    return 0
	    ;;
	-L)
	    local levels="error warning notice net info debug noise noise2 noise3"
	    COMPREPLY=( $(compgen -W "${levels}" -- ${cur}) )
	    return 0
	    ;;
	-m)
	    local modes=$(find /usr/share/scratchbox2/modes -maxdepth 1 -type d -printf '%f\n' -o -type l -printf '%f\n')
	    COMPREPLY=( $(compgen -W "${modes}" -- ${cur}) )
	    return 0
	    ;;
	*)
	    ;;
    esac

    COMPREPLY=( $(compgen -W "${opts}" -- ${cur}))
    return 0
} &&
complete -F _sb2 sb2
