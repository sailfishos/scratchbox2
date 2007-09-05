#!/bin/bash
#-------------------------------------------------------------------------------
# License
#-------------------------------------------------------------------------------

# Copyright (C) 2006 Movial
# Authors: Jussi Hakala <jussi.hakala@movial.fi>
# Licensed under GNU GPL, see COPYING for details

#-------------------------------------------------------------------------------
# Configuration
#-------------------------------------------------------------------------------

INIT_PATH_TMP='/tmp/init'

INIT_COLOR_MSG_0='\033[0;34m'
INIT_COLOR_MSG_1='\033[1;34m'

INIT_COLOR_VER_0='\033[0;32m'
INIT_COLOR_VER_1='\033[0;31m'

INIT_COLOR_NORMAL='\033[0;37m'

#-------------------------------------------------------------------------------
# Defaults
#-------------------------------------------------------------------------------

OPTION_TESTS='compile,type,strip,execute'
OPTION_COLOR=0

OPTION_VERSION=0
OPTION_USAGE=0

#-------------------------------------------------------------------------------
# Functions
#-------------------------------------------------------------------------------

function init_message
{
	local MESSAGE="$1"

	cat << EOF
$MESSAGE
EOF
}

function init_warning
{
	local MESSAGE="$1"

	init_message "WARNING: $MESSAGE"
}

function init_error
{
	local EXIT="$1"
	local MESSAGE="$2"

	init_message "ERROR: $MESSAGE"
	exit "$EXIT" 
}

function init_usage
{
	cat << EOF
Usage: init_tests.sh [-h|-t <tests>|-v]
Options: -c          colorize output
         -h          help
         -t <string> comma separated list of tests in order
                     compile,dependencies,strip,type,execute
                     c,t,d,s,x (default)
         -v          version
EOF
	exit 0
}

function init_version
{
	cat << EOF
init_tests.sh: toolchain operation testing
Version 2.0

Original author: Ricardo Kekki <ricardo.kekki@movial.fi>
Adopted by: Jussi Hakala <jussi.hakala@movial.fi>
Copyright (C) 2006 Movial
EOF

	exit 0
}

function init_dumplibc
{
	local RESULT=
	local PATH_LIBS='/lib'

	local LIBC=
	local PATH_LIBC=
	local PATH_LIBC_FOUND=
	for LIBC in libc.so.6 libc.so.0 ; do
		PATH_LIBC="${PATH_LIBS}/$LIBC"
		if [ -e "$PATH_LIBC" ] ; then
			PATH_LIBC_FOUND="$PATH_LIBC"
			break 1
		fi
	done

	if [ "#EMPTY#$PATH_LIBC_FOUND" != "#EMPTY#" ] ; then
		RESULT=`objdump -f "$PATH_LIBC_FOUND" \
			| awk -F ' |,' '/architecture/ {print $2}'`
	fi

	echo -n "$RESULT"
}

function init_dumpgcc
{
	local RESULT=

	if which gcc &> /dev/null ; then
		RESULT=`gcc -dumpmachine | cut -d - -f 1`
	fi

	echo -n "$RESULT"
}

function init_get_architecture
{
	local RESULT=
	local ARCH="$1"

	case "$ARCH" in
		arm) RESULT='ARM' ;;
		i[34]86) RESULT='Intel 80386' ;;
		mips) RESULT='MIPS' ;;
	esac

	echo -n "$RESULT"
}

function init_test_compile
{
	local RESULT=0

	# test the c compiler
	cat > "${INIT_PATH_TMP}/hello.c" <<EOF
#include <stdio.h>
int main() {
	printf("Hello world!\n");
	return 0;
}
EOF

	gcc -static "${INIT_PATH_TMP}/hello.c" \
		-o "${INIT_PATH_TMP}/hello-static" || RESULT=1
	gcc "${INIT_PATH_TMP}/hello.c" \
		-o "${INIT_PATH_TMP}/hello" || RESULT=1

	# test the c++ compiler
	cat > "${INIT_PATH_TMP}/hello.cc" <<EOF
#include <iostream>
int main() {
	std::cout << "Hello world!\n";
	return 0;
}
EOF

	g++ -static "${INIT_PATH_TMP}/hello.cc" \
		-o "${INIT_PATH_TMP}/hellocc-static" || RESULT=1
	g++ "${INIT_PATH_TMP}/hello.cc" \
		-o "${INIT_PATH_TMP}/hellocc" || RESULT=1

	return "$RESULT"
}

function init_test_execute
{
	local RESULT=0

	local REF=`echo "Hello world!"`

	local BIN=
	local OUT=
	for BIN in hello hello-static hellocc hellocc-static ; do
		OUT=`"${INIT_PATH_TMP}/$BIN" 2>&1`
		echo "$OUT"
		[ "$OUT" == "$REF" ] || RESULT=1
	done

	return "$RESULT"
}

function init_test_type
{
	local RESULT=0

	local ARCH="$1"

	local BIN=
	local TYPE=
	local MATCH=
	for BIN in hello hello-static hellocc hellocc-static ; do
		TYPE=`file "${INIT_PATH_TMP}/$BIN"`
		MATCH=`awk -v ARCH="$ARCH" -v TYPE="$TYPE" -- \
			'BEGIN { print match(TYPE, ARCH) }' | tr -d $'\n'`
		echo "$TYPE"
		[ "$MATCH" -ne 0 ] || RESULT=1
	done

	return "$RESULT"
}

function init_test_strip
{
	local RESULT=0

	local BIN=
	local OUT=
	for BIN in hello hello-static hellocc hellocc-static ; do
		OUT=`strip "${INIT_PATH_TMP}/$BIN" 2>&1`
		[ $? -eq 0 ] || RESULT=1
		[ "$OUT" ] && RESULT=1
	done

	return "$RESULT"
}

function init_do_tests
{
	local TESTS="$1"
	local ARCH="$2"

	local RESULT=0

	local VERB_0='OK'
	local VERB_1='ERROR'

	IFS=,
	local TEST=
	local TEST_RESULT=
	for TEST in $TESTS ; do
		TEST_RESULT=0
		echo -e "${INIT_COLOR_MSG_0}Starting test: ${INIT_COLOR_MSG_1}${TEST}${INIT_COLOR_NORMAL}"
		case $TEST in
			c|C|compile)
				init_test_compile
				[ $? -ne 0 ] && TEST_RESULT=1
				;;
			d|D|dependencies)
				init_test_dependencies
				[ $? -ne 0 ] && TEST_RESULT=1
				;;
			s|S|strip)
				init_test_strip
				[ $? -ne 0 ] && TEST_RESULT=1
				;;
			t|T|type)
				init_test_type "$ARCH"
				[ $? -ne 0 ] && TEST_RESULT=1
				;;
			x|X|execute)
				init_test_execute
				[ $? -ne 0 ] && TEST_RESULT=1
				;;
			*)
				init_warning "Unknown test, skipping..."
				TEST_RESULT=1
				;;
		esac
		echo -ne "${INIT_COLOR_MSG_0}Finishing test: ${INIT_COLOR_MSG_1}${TEST}${INIT_COLOR_MSG_0}, result: "
		eval "echo -e \${INIT_COLOR_VER_$TEST_RESULT}\$VERB_$TEST_RESULT\${INIT_COLOR_NORMAL}"
		[ "$TEST_RESULT" -ne 0 ] && RESULT=1
	done
	IFS=$'\n\t '

	return "$RESULT"
}

function init_exit
{
	echo rm -rf "$INIT_PATH_TMP" &> /dev/null
}

#-------------------------------------------------------------------------------
# Main
#-------------------------------------------------------------------------------

while getopts ":cht:v" OPTION ; do
	case $OPTION in
		c)
			OPTION_COLOR=1
			;;
		h)
			OPTION_USAGE=1
			;;
		t)
			OPTION_TESTS="$OPTARG"
			;;
		v)
			OPTION_VERSION=1
			;;
		*)
			init_error 1 "Invalid argument(s). Try $0 -h for help."
			;;
	esac
done

if [ "$OPTION_COLOR" -ne 1 ] ; then
	INIT_COLOR_MSG_0=
	INIT_COLOR_MSG_1=
	INIT_COLOR_VER_0=
	INIT_COLOR_VER_1=
	INIT_COLOR_NORMAL=
fi

if [ "$OPTION_USAGE" -ne 0 ] ; then
	init_usage
fi

if [ "$OPTION_VERSION" -ne 0 ] ; then
	init_version
fi

TYPE=`init_dumplibc`
if [ "#EMPTY#$TYPE" == "#EMPTY#" ] ; then
	TYPE=`init_dumpgcc`
	if [ "#EMPTY#$TYPE" == "#EMPTY#" ] ; then
		init_error 1 "Cannot determine the architecture"
	fi
fi

ARCH=`init_get_architecture "$TYPE"`

trap init_exit EXIT

rm -rf "$INIT_PATH_TMP" &> /dev/null
mkdir -p "$INIT_PATH_TMP"

init_do_tests "$OPTION_TESTS" "$ARCH"
STATUS=$?

exit "$STATUS"

#-------------------------------------------------------------------------------
# EOF
#-------------------------------------------------------------------------------

