#!/bin/bash

if [ -z "$SBOX_TOOLS_ROOT" ]; then
	echo "SBOX_TOOLS_ROOT: $SBOX_TOOLS_ROOT"
	export SBOX_TOOLS_ROOT=/scratchbox/sarge
fi

export REDIR_LD_LIBRARY_PATH=$SBOX_TOOLS_ROOT/usr/lib:$SBOX_TOOLS_ROOT/lib:$SBOX_TOOLS_ROOT/usr/local/lib
export REDIR_LD_SO=$SBOX_TOOLS_ROOT/lib/ld-linux.so.2

echo "Starting Scratchbox 2..."
echo "Using sudo, type your password:"

sudo ./scratchbox/bin/sb2init

