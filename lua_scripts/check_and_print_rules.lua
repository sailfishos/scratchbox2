-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- A Lua script for checking the rules.
-- Usage:
--	sb2 sb2-show execluafile lua_scripts/check_and_print_rules.lua
--	sb2 -e sb2-show execluafile lua_scripts/check_and_print_rules.lua
--	sb2 -m tools sb2-show execluafile lua_scripts/check_and_print_rules.lua
--	...

valid_keywords_in_rules = {
	
	-- Name of the rule (a comment in practice):
	name = "string",

	-- ========= Fields used by find_rule() =========

	-- first, the three selectors:
	-- (there should be only one of these, but this
	-- script does not check that - yet)
	dir = "string",
	path = "string",
	prefix = "string",

	-- additional conditions for rule selection:
	binary_name = "string",
	func_name = "string",

	-- a subtree:
	rules = "table",

	-- ========= Fields used to translate the path =========
	-- (sbox_translate_path(), sbox_execute_rule())

	-- Logging:
        log_level = "string",
	log_message = "string",

	-- simple actions:
	-- (there should be only one action, but this
	-- script does not check that - yet)
        replace_by = "string",
        map_to = "string",
	use_orig_path = "boolean",
	force_orig_path = "boolean",
	union_dir = "table",
	custom_map_funct = "function",

	-- action lists; allows use of conditionals (see
	-- 'valid_keywords_in_actions' below):
	actions = "table",

	-- additional attributes:
	readonly = "boolean",
	exec_policy_name = "string",

	-- ========= Fields used by the reverse rule generator =========
	virtual_path = "boolean",

}

valid_keywords_in_actions = {

	-- Logging:
        log_level = "string",
	log_message = "string",

	-- conditionals:
	-- (these are handled in sbox_execute_conditional_actions())
	if_exists_then_replace_by = "string",
	if_exists_then_map_to = "string",
	if_active_exec_policy_is = "string",
	if_redirect_ignore_is_active = "string",
	if_redirect_force_is_active = "string",

	-- unconditional actions:
	-- (handled by calling sbox_execute_rule())
	-- (there should be only one action, but this
	-- script does not check that - yet)
        replace_by = "string",
        map_to = "string",
	use_orig_path = "boolean",
	force_orig_path = "boolean",

	-- additional attributes:
	readonly = "boolean",
	exec_policy_name = "string",
}


num_errors = 0

function log_error(errtxt)
	print("--ERROR: "..errtxt)
	num_errors = num_errors + 1
end

function check_rule_node(name, indent, idx, tag, val)
	if type(val) == "table" then
		print(indent.."   "..tag.." = {")
		if tag == "union_dir" then
			for idx, path in pairs(val) do
				if type(path) ~= "string" then
					log_error("a "..type(path)..
						" was found in an union_dir "..
						"array (all components must be strings)")
				else
					print(indent.."      '"..path.."'")
				end
			end
		elseif tag == "actions" then
			check_actions_list(name.."->"..idx, indent.."          ", val)
		elseif tag == "rules" then
			check_rule_list(name.."->"..idx, indent.."          ", val)
		else
			log_error("Unknown table "..tag)
		end
		print(indent.."   }")
	elseif type(val) == "string" then
		print(indent.."   "..tag.." = '"..val.."'")
	elseif type(val) == "number" then
		print(indent.."   "..tag.." = "..val)
	elseif type(val) == "boolean" then
		if val then
			print(indent.."   "..tag.." = true")
		else
			print(indent.."   "..tag.." = false")
		end
	elseif type(val) == "function" then
		-- FIXME: print something sensible about the function
		print(indent.."   -- FUNCTION: "..tag)
	else
		log_error("Unsupported type ("..tag.." is a "..type(val)..")")
	end
end

function check_list_of_nodes(name, indent, tbl, keyword_table)
	print(indent.."-- "..name..":")
	for idx, rule in pairs(tbl) do
		-- idx is numeric here.
		print(indent.."{")
		for tag,val in pairs(rule) do

			required_type = keyword_table[tag]
			if required_type then
				if type(val) == required_type then
					check_rule_node(name,indent,idx,tag,val)
				else
					log_error("Type of field '"..tag..
						"' should be "..required_type..
						" but a "..type(val).." was found.")
				end
			else
				log_error("Invalid keyword: "..tag)
			end
		end
		print(indent.."}")
	end
end

function check_actions_list(name, indent, tbl)
	check_list_of_nodes(name.."(actions)", indent, tbl, valid_keywords_in_actions)
end

function check_rule_list(name, indent, tbl)
	check_list_of_nodes(name, indent, tbl, valid_keywords_in_rules)
end

print("fs_mapping_rules = {")
check_rule_list("fs_mapping_rules", "  ", fs_mapping_rules)
print("}")

if num_errors == 0 then
	print("-- Rules OK, no errors")
else
	print("-- "..num_errors.." ERRORS FOUND.")
end

