# Init script for Scratchbox debugging -*- gdb-script -*-
#
# SPDX-FileCopyrightText: Copyright (C) Jolla Ltd
# Author: Björn Kettunen
#
# SPDX-License-Identifier: GPL-2.0-or-later

define prepare-libsb2-debug-session
  set follow-fork-mode child
  eval "set exec-wrapper sb2 %s", $SB2_ARGS
  _sb2_show_start
end
document prepare-libsb2-debug-session
Prepare session to debug session of libsb2.

Sets up so libsb2 can be debugged in gdb.

Important GDB has to run outside of Scratchbox.
sb2 has to be in $PATH.
end

set $SB2_SHOW_START = "/bin/ls"

define _sb2_show_start
  file /usr/bin/sb2-show
  eval "set args start -- %s", $SB2_SHOW_START
end

define prepare-libsb2-host-debug-session
  prepare-libsb2-debug-session
  add-symbol-file build/preload/libsb2.so
end

document prepare-libsb2-host-debug-session
Prepare debugging session of the host's libsb2.

Just like `prepare-libsb2-debug-session' but setup
debug session further to start sb2-show to start a test program.

Default test program is $SB2_SHOW_START i.e. /bin/ls
end

# set $SBOX_DEBUG_SESSION_DIR = pwd + "/sb2-debug/session-dir"

define prepare-early-libsb2-debug-session
  shell sb2 -E --init-session-only -W $SBOX_DEBUG_SESSION_DIR -S $SBOX_DEBUG_SESSION_FILEO
  eval "set exec-wrapper sb2 -J %s", $SBOX_DEBUG_SESSION_FILE
  _sb2_show_start
end
document prepare-early-libsb2-debug-session
Prepare debug session for debugging libsb2 before session startup.

Useful when libsb2 crashes early.
No CPU transparency available.

Sets up a permanent session dir in $SBOX_DEBUG_SESSION_DIR.
end

python

import os
gdb.execute("set $SBOX_DEBUG_SESSION_DIR  = \"" + os.getcwd() + "/sb2-debug/session-dir\"")
gdb.execute("set $SBOX_DEBUG_SESSION_FILE = \"" + os.getcwd() + "/sb2-debug/session-file\"")
end

# GDB mishandles indentation with leading tabs when feeding it to Python.
# Local Variables:
# indent-tabs-mode: nil
# End:
