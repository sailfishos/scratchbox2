#!/bin/bash

export SBOX_TOOLS_ROOT=""

export REDIR_LD_LIBRARY_PATH=$SBOX_TOOLS_ROOT/usr/lib:$SBOX_TOOLS_ROOT/lib:$SBOX_TOOLS_ROOT/usr/local/lib
export REDIR_LD_SO=$SBOX_TOOLS_ROOT/lib/ld-linux.so.2

echo "Starting Scratchbox 2..."

./scratchbox/bin/sb2

