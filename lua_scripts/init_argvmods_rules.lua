-- Copyright (c) 2009,2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.
--
-- Read in argvmods_{gcc,misc}.lua files and insert
-- exec preprocessing rules to the rule tree db.
--
-- This script reads in argvmods_xxx.lua file (xxx being here
-- gcc or misc) and writes out lua table containing generated
-- argvmods rules.
--
-- argv&envp mangling rules are separated into two files
-- 	argvenvp_misc.lua - rules for misc binaries
-- 	argvenvp_gcc.lua  - rules for gcc
--
-- With these rules, this script generates exec preprocessing
-- rules, and writes those to SBOX_SESSION_DIR/argvmods_misc.lua and
-- SBOX_SESSION_DIR/argvmods_gcc.lua.
--
-- Syntax is of the form:
--
-- rule = {
--	path_prefixes = {"/list/of", "/possible/path/prefixes"},
-- 	add_head = {"list", "of", "args", "to", "prepend"},
-- 	add_options = {"list", "of", "options", "to", "add",
--		 "after", "argv[0]"},
-- 	add_tail = {"these", "are", "appended"},
-- 	remove = {"args", "to", "remove"},
-- 	new_filename = "exec-this-binary-instead",
-- 	disable_mapping = 1 -- set this to disable mappings
-- }
-- argvmods[name] = rule
--
-- Environment modifications are not supported yet, except for disabling
-- mappings.

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
	for k = 1, #stringlist do
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

-- contents => variable name lookup table for the old-style table
-- FIXME: This is almost obsolete, can be removed after 
-- create_argvmods_usr_bin_rules.lua has been replaced
-- completely, and files are not anymore used to store rules
-- (currently all functionality from create_argvmods_usr_bin_rules.lua
-- has been moved to init_autogen_usr_bin_rules.lua, but the
-- algorithm is still the same => this code here is still needed.
-- A bigger code cleanup is needed.)
local table2tablename = {}
local table2tablename_n = 0

-- FIXME: This is almost obsolete, can be removed after 
-- create_argvmods_usr_bin_rules.lua has been replaced.
-- (see comment above)
function clear_attr_list()
	table2tablename = {}
	table2tablename_n = 0
end

-- FIXME: This is almost obsolete, can be removed after 
-- create_argvmods_usr_bin_rules.lua has been replaced.
-- (see comment above)
function prepare_stringlist(filehandle, name, stringlist)
	if stringlist and #stringlist > 0 then
		local x = stringlist_to_string(stringlist)
		local key = name.."\n"..x
		if table2tablename[key] == nil then
			local z_name = string.format("z_argvmods_%s_%d",
				name, table2tablename_n)
			table2tablename[key] = z_name
			table2tablename_n = table2tablename_n + 1
			
			filehandle:write(z_name.." = {\n"..x.."}\n")
		end
	end
end

-- FIXME: This is almost obsolete, can be removed after 
-- create_argvmods_usr_bin_rules.lua has been replaced.
-- (see comment above)
function find_stringlist_name(name, stringlist)
	local stringlist_name = ""
	if stringlist and #stringlist > 0 then
		local x = stringlist_to_string(stringlist)
		local key = name.."\n"..x
		return table2tablename[key]
	end
	return stringlist_name
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

-- This function creates the old-style argvmods_*.lua files.
-- FIXME: This is almost obsolete, can be removed after 
-- create_argvmods_usr_bin_rules.lua has been replaced.
-- (see comment above)
function argvmods_to_file(filename, num_argvmods_rules, argvmods_tbl)
	argvmods_file = io.open(filename, "w")
	if not argvmods_file then
		io.stderr:write(string.format(
		    "Failed to open '%s' for writing\n", filename))
		return
	end
	argvmods_file:write(
		"--\n"..
		"-- Generator: init_argvmods_rules.lua\n"..
		"-- Automatically generated rules.  Do not modify:\n"..
		"--\n")
	for binary_name, rule in pairs(argvmods_tbl) do
		for name, value in pairs(rule) do
			if type(value) == "table" then
				if #value ~= 0 then
					prepare_stringlist(argvmods_file, name, value)
				end
			end
		end
	end
	argvmods_file:write(
		"argvmods = {\n")
	for binary_name, rule in pairs(argvmods_tbl) do
		argvmods_file:write(string.format("\t[\"%s\"] = {\n", binary_name))
		for name, value in pairs(rule) do
			if name == "name" then
				-- "name" field is redundant, skip it
			else
				argvmods_file:write(string.format("\t%s = ", name))
				if type(value) == "string" then
					argvmods_file:write(string.format("\"%s\"", value))
				elseif type(value) == "number" then
					argvmods_file:write(string.format("%d", value))
				elseif type(value) == "table" then
					if #value == 0 then
						argvmods_file:write("{}")
					else
						argvmods_file:write(find_stringlist_name(name,value))
					end
				else
					io.stderr:write(string.format(
					    "unsupported type '%s' in argvmods rule\n",
					    type(value)))
					assert(false)
				end
				argvmods_file:write(",\n")
			end
		end
		argvmods_file:write("\t},\n")
	end
	argvmods_file:write(
		"}\n"..
		"--\n"..
		string.format("-- Total %d rule(s).\n", num_argvmods_rules)..
		"-- End of rules created by argvmods expander.\n"..
		"--\n")
	argvmods_file:close()
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
argvmods_to_file(session_dir .. "/argvmods_misc.lua", num_rules, argvmods)
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
clear_attr_list()
argvmods_to_file(session_dir .. "/argvmods_gcc.lua", num_rules, argvmods)
if (debug_messages_enabled) then
	sblib.log("debug",
		string.format("%d rules", num_rules))
end

argvmods = nil  -- cleanup.
