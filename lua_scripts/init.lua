-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- License: LGPL-2.1

-- This script is executed by sb2d at startup, this
-- initializes the rule tree database (which is empty
-- but attached when this script is started)

session_dir = os.getenv("SBOX_SESSION_DIR")
debug_messages_enabled = sblib.debug_messages_enabled()

-- This version string is used to check that the lua scripts offer 
-- what sb2d expects, and v.v.
-- Increment the number whenever the interface beween Lua and C is changed.
--
-- NOTE: the corresponding identifier for C is in include/sb2.h,
-- see that file for description about differences
sb2d_lua_c_interface_version = "301"

-- Create the "vperm" catalog
--	vperm::inodestats is the binary tree, initially empty,
--	but the entry must be present.
--	all counters must be present and zero in the beginning.
ruletree.catalog_set("vperm", "inodestats", 0)
ruletree.catalog_set("vperm", "num_active_inodestats",
	ruletree.new_uint32(0))

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
		return f() -- execute the loaded chunk
	end
end

function import_from_fs_rule_library(libname)
	local libpath = session_dir.."/rule_lib/fs_rules/"..libname..".lua"
	local fs_rules
	fs_rule_lib_interface_version = nil
	fs_rules = do_file(libpath)
	if fs_rule_lib_interface_version == "105" then
		return fs_rules
	end
	error("\nIncorrect fs_rule_lib_interface_version while loading FS rule library "..libname.."\n")
end

-- A dummy sb2_procfs_mapper function is needed for the rules,
-- now when the real function is gone
function sb2_procfs_mapper()
	return true
end

-- Other utility functions for the mapping rules:
function basename(path)
	if (path == "/") then
		return "/"
	else
		return string.match(path, "[^/]*$")
	end
end

-- Load session-specific settings
do_file(session_dir .. "/sb2-session.conf")

target_root = sbox_target_root
if (not target_root or target_root == "") then
	target_root = "/"
end

tools_root = sbox_tools_root
if (tools_root == "") then
	tools_root = nil
end

tools = tools_root
if (not tools) then
	tools = "/"
end

if (tools == "/") then
        tools_prefix = ""
else
        tools_prefix = tools
end

-- Load session configuration, and add variables to ruletree.
do_file(session_dir .. "/sb2-session.conf")

ruletree.catalog_set("config", "sbox_cpu",
        ruletree.new_string(sbox_cpu))
ruletree.catalog_set("config", "sbox_uname_machine",
        ruletree.new_string(sbox_uname_machine))
ruletree.catalog_set("config", "sbox_emulate_sb1_bugs",
        ruletree.new_string(sbox_emulate_sb1_bugs))

-- Load exec config.
-- NOTE: At this point all conf_cputransparency_* variables
-- are still missing from exec_config.lua. Other variables
-- (conf_tools_*, conf_target_*, host_*) are there.
do_file(session_dir .. "/lua_scripts/exec_constants.lua")
do_file(session_dir .. "/exec_config.lua")

-- Add exec config parameters to ruletree:
ruletree.catalog_set("config", "host_ld_preload", ruletree.new_string(host_ld_preload))
ruletree.catalog_set("config", "host_ld_library_path", ruletree.new_string(host_ld_library_path))

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

-- mode-spefic config to ruletree:
do_file(session_dir .. "/lua_scripts/init_modeconfig.lua")

-- Create rules based on "argvmods", e.g. rules for toolchain components etc.
do_file(session_dir .. "/lua_scripts/init_autogen_usr_bin_rules.lua")

-- Create reverse mapping rules.
do_file(session_dir .. "/lua_scripts/create_reverse_rules.lua")

-- Now add all mapping rules to ruletree:
do_file(session_dir .. "/lua_scripts/add_rules_to_rule_tree.lua")

-- and networking rules:
do_file(session_dir .. "/lua_scripts/init_net_modes.lua")

-- Done. conf_cputransparency_* are still missing from the rule tree,
-- but those can't be added yet (see the "sb2" script - it finds
-- values for those variables only after sb2d has executed this
-- script)

io.stdout:flush()
io.stderr:flush()
