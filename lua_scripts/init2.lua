-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.

-- This script is executed by sb2d when "init2" message is received.
-- The "sb2" script sends that to finalize initializations.

session_dir = os.getenv("SBOX_SESSION_DIR")
debug_messages_enabled = sblib.debug_messages_enabled()

print("--- init2 ---")

-- Done.

init2_result = "All is OK, init2 didn't have to do anything yet."

io.stdout:flush()
io.stderr:flush()
