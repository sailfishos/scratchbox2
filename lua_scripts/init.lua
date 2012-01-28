-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.

-- This script is executed by sb2d at startup, this
-- initializes the rule tree database (which is empty
-- but attached when this script is started)

session_dir = os.getenv("SBOX_SESSION_DIR")
debug_messages_enabled = sblib.debug_messages_enabled()

function do_file(filename)
	if (debug_messages_enabled) then
		sblib.log("debug", string.format("Loading '%s'", filename))
	end
	local f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" 
			.. err .. "\n")
		-- "error()" never returns
	else
		f() -- execute the loaded chunk
	end
end

-- Build "all_modes" table. all_modes[1] will be name of default mode.
all_modes_str = os.getenv("SB2_ALL_MODES")
all_modes = {}

if (all_modes_str) then
	for m in string.gmatch(all_modes_str, "[^ ]*") do
		if m ~= "" then
			table.insert(all_modes, m)
			ruletree.catalog_set("MODES", m, 0)
		end
	end
end

ruletree.catalog_set("MODES", "#default", ruletree.new_string(all_modes[1]))

-- Exec preprocessing rules to ruletree:
do_file(session_dir .. "/lua_scripts/init_argvmods_rules.lua")

