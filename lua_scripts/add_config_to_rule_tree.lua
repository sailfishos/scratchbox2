-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed when SB2 session is created,
-- to insert configuration variable values to the rule tree database.

if not mapping_engine_loaded then
	do_file(session_dir .. "/lua_scripts/mapping.lua")
end

-- Load session-specific settings
session_dir = sb.get_session_dir()
do_file(session_dir .. "/sb2-session.conf")

ruletree.attach_ruletree()

ruletree.catalog_set("config", "sbox_cpu",
	ruletree.new_string(sbox_cpu))
ruletree.catalog_set("config", "sbox_uname_machine",
	ruletree.new_string(sbox_uname_machine))
ruletree.catalog_set("config", "sbox_emulate_sb1_bugs",
	ruletree.new_string(sbox_emulate_sb1_bugs))

