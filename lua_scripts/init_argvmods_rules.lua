-- Copyright (c) 2009,2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.
--
-- Read in argvmods_{gcc,misc}.lua files and insert
-- exec preprocessing rules to the rule tree db.

local argvmods_allowed_rulenames = {
	"name",
	"path_prefixes",
	"add_head",
	"add_options",
	"add_tail",
	"remove",
	"new_filename",
	"disable_mapping",
	"drivers",
}

function check_and_count_rules(rule_table)
	local num_rules = 0;
	for binary_name, rule in pairs(rule_table) do
		for name, value in pairs(rule) do
			--
			-- Check that rulename is valid.
			--
			local found = 0
			for i = 1, #argvmods_allowed_rulenames do
				if name == argvmods_allowed_rulenames[i] then
					found = 1
					break
				end
			end
			if found == 0 then
				io.stderr:write(string.format(
				    "invalid rulename '%s'\n", name))
			end
		end
		num_rules = num_rules + 1
	end
	return(num_rules)
end

function stringlist_to_ruletree(stringlist)
	local stringlist_index = ruletree.objectlist_create(#stringlist)
	for k = 1, table.maxn(stringlist) do
		local string_index = ruletree.new_string(stringlist[k])
		ruletree.objectlist_set(stringlist_index, k-1, string_index)
	end
	return stringlist_index
end

function stringlist_to_string(stringlist)
	local s = ""
	for i = 1, #stringlist do
		assert(type(stringlist[i]) == "string")

		s = s..string.format("\t\"%s\",\n", stringlist[i])
	end
	return s
end

local table2rule_tree_offs = {}

function find_offs_for_stringlist(name, stringlist)
	local stringlist_offs = 0
	if stringlist and #stringlist > 0 then
		local x = stringlist_to_string(stringlist)
		local key = name.."\n"..x
		if table2rule_tree_offs[key] == nil then
			table2rule_tree_offs[key] = stringlist_to_ruletree(stringlist)
		end
		stringlist_offs = table2rule_tree_offs[key]
	end
	return stringlist_offs
end

function argvmods_to_ruletree(argvmods_mode_name, num_argvmods_rules, argvmods_tbl)
	local argvmods_rule_list_index = ruletree.objectlist_create(num_argvmods_rules)
	local k = 0;
	for binary_name, rule in pairs(argvmods) do
		local path_prefixes_table_offs = find_offs_for_stringlist("path_prefixes", rule.path_prefixes)
		local add_head_table_offs = find_offs_for_stringlist("add_head", rule.add_head)
		local add_options_table_offs = find_offs_for_stringlist("add_options", rule.add_options)
		local add_tail_table_offs = find_offs_for_stringlist("add_tail", rule.add_tail)
		local remove_table_offs = find_offs_for_stringlist("remove", rule.remove)
		local rule_index = ruletree.add_exec_preprocessing_rule_to_ruletree(
                        binary_name, path_prefixes_table_offs,
                        add_head_table_offs, add_options_table_offs,
                        add_tail_table_offs, remove_table_offs,
                        rule.new_filename, rule.disable_mapping)
		ruletree.objectlist_set(argvmods_rule_list_index, k, rule_index)
		k = k + 1
	end
	ruletree.catalog_set("argvmods", argvmods_mode_name, argvmods_rule_list_index)
end

if (debug_messages_enabled) then
	sblib.log("debug",
		string.format("Adding exec preprocessing rules ('argvmods') to ruletree"))
end

-- first "misc":
argvmods = {}
argvmods_source_file = session_dir .. "/lua_scripts/argvenvp_misc.lua"
do_file(argvmods_source_file)
local num_rules = check_and_count_rules(argvmods)
argvmods_to_ruletree("misc", num_rules, argvmods)
if (debug_messages_enabled) then
	sblib.log("debug",
		string.format("%d rules", num_rules))
end

-- Next "gcc" and "misc";
-- misc rules are already in argvmods table, don't clear it
argvmods_source_file = session_dir .. "/lua_scripts/argvenvp_gcc.lua"
do_file(argvmods_source_file)
local num_rules = check_and_count_rules(argvmods)
argvmods_to_ruletree("gcc", num_rules, argvmods)
if (debug_messages_enabled) then
	sblib.log("debug",
		string.format("%d rules", num_rules))
end

-- Modename => argvmods type table
enable_cross_gcc_toolchain = true

for m_index,m_name in pairs(all_modes) do
	do_file(session_dir .. "/share/scratchbox2/modes/"..m_name.."/config.lua")

	ruletree.catalog_set("use_gcc_argvmods", m_name,
		ruletree.new_boolean(enable_cross_gcc_toolchain))
end

