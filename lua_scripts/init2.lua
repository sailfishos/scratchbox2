-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.

-- This script is executed by sb2d when "init2" message is received.
-- The "sb2" script sends that to finalize initializations.

session_dir = os.getenv("SBOX_SESSION_DIR")
debug_messages_enabled = sblib.debug_messages_enabled()

-- Default:
init2_result = "OK - CPU transparency settings loaded."
init2_errors = ""

print("--- init2 ---")

local valid_keywords_for_cpu_transparency_rule = {
	cmd = "string",
	arch = "string",
	has_argv0_flag = "boolean",
	qemu_has_env_control_flags = "boolean",
	qemu_has_libattr_hack_flag = "boolean",
	qemu_ld_library_path = "string",
	qemu_ld_preload = "string",
	qemu_argv = "table",
	qemu_env = "table",
}

function add_to_cputr_config(configsetname, key, t, val)
	io.stderr:write(string.format(
		"add_to_cputr_config %s %s %s\n", configsetname, key, t))

	if t == "string" then
		ruletree.catalog_vset("cputransparency", configsetname,
			key, ruletree.new_string(val))
	elseif t == "boolean" then
		ruletree.catalog_vset("cputransparency", configsetname,
			key, ruletree.new_boolean(val))
	elseif t == "table" then
		local list_index = ruletree.objectlist_create(#val)
		for i = 1, table.maxn(val) do
			local t2 = type(val[i])
			if t2 == "string" then
				local new_str_index = ruletree.new_string(val[i])
				ruletree.objectlist_set(list_index, i-1, new_str_index)
			else
				init2_errors = init2_errors .. "Member of table "..key.." is not a string. "
			end
		end
		ruletree.catalog_vset("cputransparency", configsetname,
			key, list_index)
	else
		init2_errors = init2_errors .. "Unsupported type "..key.."="..t..". "
	end
end

function add_cputr_settings(configsetname, tbl)
	io.stderr:write(string.format(
		"add_cputr_settings %s\n", configsetname))
	for key,val in pairs(tbl) do
		local required_type = valid_keywords_for_cpu_transparency_rule[key]
		local t = type(val)
		if required_type then
			if t == required_type then
				add_to_cputr_config(configsetname, key, t, val)
			else
				init2_errors = init2_errors .. "Field "..key.." has wrong type. "
			end
		else
			init2_errors = init2_errors .. "Illegal field "..key..". "
		end
	end
end

-- Add CPU transparency settings to the rule tree
conf_cputransparency_target = nil
conf_cputransparency_native = nil
do_file(session_dir .. "/cputransp_config.lua")

if conf_cputransparency_target ~= nil then
	add_cputr_settings("target", conf_cputransparency_target)
end
if conf_cputransparency_native ~= nil then
	add_cputr_settings("native", conf_cputransparency_native)
end

-- Done.
if init2_errors ~= "" then
	init2_result = "Errors in CPU transparency config: "..init2_errors
end

io.stdout:flush()
io.stderr:flush()


