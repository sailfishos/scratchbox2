-- Copyright (c) 2009,2012 Nokia Corporation.
-- Copyright (c) 2021      Jolla Ltd.
-- Author: Lauri T. Aarnio
--
-- License: LGPL-2.1

-- This script is executed by sb2d, while initializing a new session,
-- to create mapping rules for toolchain components:
-- For example, /usr/bin/gcc will be mapped to the toolchain. These rules
-- are used for other filesystem operations than exec*
-- (for example, when the shell is looking for a program, it must be
-- possible to stat() the destination)
--
-- This file is based on "create_argvmods_usr_bin_rules.lua", which
-- did the same thing (but was executed to process just one mapping mode,
-- from the "sb2" script during session setup) 

function argvmods_to_mapping_rules(rule_file, prefix)
	local n
	for n in pairs(argvmods) do
		local rule = argvmods[n]
		-- rule_file:write("-- rule ", n, " new_filename=", rule.new_filename.."\n")
		local process_now = true
		if prefix ~= nil then
			if not sblib.isprefix(prefix, n) then
				process_now = false
			end
		end

		if process_now and not rule.argvmods_processed then
			rule.argvmods_processed = true
			local k
			for k=1,#rule.path_prefixes do
				if rule.new_filename ~= nil then
					-- this rule maps "n" from /usr/bin to
					-- another file
					if sblib.path_exists(rule.new_filename) then
						rule_file:write("  {path=\""..rule.path_prefixes[k]..n..
							"\",\n")
						rule_file:write("   replace_by=\"" ..
							rule.new_filename.."\"},\n")
					else
						rule_file:write("  -- WARNING: " ..
							rule.new_filename ..
							" does not exist\n")
					end
				end
			end
		end
	end
end

function create_mapping_rule_file(filename, modename_in_ruletree)

	local rule_file = io.open(filename, "w")
        if not rule_file then
                io.stderr:write(string.format(
                    "Failed to open '%s' for writing\n", filename))
                return
        end

	rule_file:write("-- Argvmods-to-mapping-rules converter:\n")
	rule_file:write("-- Automatically generated mapping rules. Do not edit:\n")

	-- Mode-specific fixed config settings to ruledb:
	--
	-- "enable_cross_gcc_toolchain" (default=true): All special processing
	--     for the gcc-related tools (gcc,as,ld,..) will be disabled if set
	--     to false.
	--
	enable_cross_gcc_toolchain = true

	tools = tools_root
	if (not tools) then
		tools = "/"
	end

	if (tools == "/") then
		tools_prefix = ""
	else
		tools_prefix = tools
	end

	do_file(session_dir .. "/share/scratchbox2/modes/"..modename_in_ruletree.."/config.lua")

	ruletree.catalog_set("Conf."..modename_in_ruletree, "enable_cross_gcc_toolchain",
		ruletree.new_boolean(enable_cross_gcc_toolchain))

	if exec_engine_loaded then
		rule_file:write("-- Warning: exec engine was already loaded, will load again\n")
	end
	-- load the right argvmods_* file
	do_file(session_dir .. "/lua_scripts/argvmods_loader.lua")
	load_argvmods_file(modename_in_ruletree)

	-- Next, the argvmods stuff.

	rule_file:write("argvmods_rules_for_usr_bin_"..sbox_cpu.." = {\n")
	argvmods_to_mapping_rules(rule_file, sbox_cpu)

	rule_file:write("}\n")
	local prefixrule1 = "  {prefix=\"/usr/bin/"..sbox_cpu..
		"\",rules=argvmods_rules_for_usr_bin_"..sbox_cpu.."},"
	local prefixrule2 = ""

	if sbox_cpu ~= sbox_uname_machine then
		rule_file:write("argvmods_rules_for_usr_bin_"..sbox_uname_machine.." = {\n")
		argvmods_to_mapping_rules(rule_file, sbox_uname_machine)
		rule_file:write("}\n")
		prefixrule2 = "  {prefix=\"/usr/bin/"..sbox_uname_machine..
			"\",rules=argvmods_rules_for_usr_bin_"..sbox_uname_machine.."},"
	end

	rule_file:write("argvmods_rules_for_usr_bin = {\n")
	rule_file:write(prefixrule1.."\n")
	rule_file:write(prefixrule2.."\n")
	argvmods_to_mapping_rules(rule_file, nil)
	rule_file:write("}\n")

	rule_file:write("-- End of rules created by argvmods-to-mapping-rules converter.\n")

	rule_file:close()
end

for m_index,m_name in pairs(all_modes) do
	local usr_bin_rules_flagfile = session_dir .. "/rules_auto/" ..
		m_name .. ".create_usr_bin_rules"
	local output_filename = session_dir .. "/rules_auto/" ..
		m_name .. ".usr_bin.lua"
	local ff = io.open(usr_bin_rules_flagfile, "r")
	if ff ~= nil then
		ff:close()
		create_mapping_rule_file(output_filename, m_name)
	else
		-- create an empty file.
		local rule_file = io.open(output_filename, "w")
		rule_file:close()
	end
end

