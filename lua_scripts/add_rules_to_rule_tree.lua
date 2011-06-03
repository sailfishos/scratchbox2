-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed when SB2 session is created,
-- to load FS mapping rules to the "rule tree" database.

-- Rule tree constants. These must match the #defines in <rule_tree.h>
local RULE_SELECTOR_PATH = 101
local RULE_SELECTOR_PREFIX = 102
local RULE_SELECTOR_DIR = 103

local RULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE = 200
local RULE_ACTION_USE_ORIG_PATH = 201
local RULE_ACTION_FORCE_ORIG_PATH = 202
local RULE_ACTION_MAP_TO = 210
local RULE_ACTION_REPLACE_BY = 211
local RULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR = 212
local RULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR = 213
local RULE_ACTION_CONDITIONAL_ACTIONS = 220
local RULE_ACTION_SUBTREE = 230
local RULE_ACTION_IF_EXISTS_THEN_MAP_TO = 245
local RULE_ACTION_IF_EXISTS_THEN_REPLACE_BY = 246
local RULE_ACTION_PROCFS = 250

local RULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS = 301
local RULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE = 302
local RULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE = 303
local RULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY = 304
local RULE_CONDITION_IF_ENV_VAR_IS_EMPTY = 305

-- and these  must match the flag definitions in mapping.h:
local RULE_FLAGS_READONLY = 1
local RULE_FLAGS_CALL_TRANSLATE_FOR_ALL = 2
local RULE_FLAGS_FORCE_ORIG_PATH = 4
local RULE_FLAGS_READONLY_FS_IF_NOT_ROOT = 8
local RULE_FLAGS_READONLY_FS_ALWAYS = 16


function get_rule_tree_offset_for_rule_list(rules, node_type_is_ordinary_rule)
	if rules[1]._rule_tree_offset == nil then
		-- Not yet in the tree, add it.
		rules[1]._rule_tree_offset = add_list_of_rules(rules, node_type_is_ordinary_rule)
	else
print ("get..Return existing at ", rules[1]._rule_tree_offset)
	end
	return rules[1]._rule_tree_offset
end

-- Add a rule to the rule tree, return rule offset in the file.
function add_one_rule_to_rule_tree(rule, node_type_is_ordinary_rule)
	local action_type = 0
	local action_str = nil
	local name
	local rule_list_link = 0

	if (rule.name == nil) then
		name = ""
	else
		name = rule.name
	end

	print(string.format("\t-- name=\"%s\",", name))

	-- Actions
	-- Unconditional actions first
	if (rule.use_orig_path) then
		action_type = RULE_ACTION_USE_ORIG_PATH
	elseif (rule.force_orig_path) then
		action_type = RULE_ACTION_FORCE_ORIG_PATH
	elseif (rule.map_to) then
		action_type = RULE_ACTION_MAP_TO
		action_str = rule.map_to
	elseif (rule.replace_by) then
		action_type = RULE_ACTION_REPLACE_BY
		action_str = rule.replace_by
	elseif (rule.map_to_value_of_env_var) then
		action_type = RULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR
		action_str = rule.map_to_value_of_env_var
	elseif (rule.replace_by_value_of_env_var) then
		action_type = RULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR
		action_str = rule.replace_by_value_of_env_var
	elseif (rule.union_dir) then
		-- FIXME: Add union_dir support.
		action_type = RULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE
	else
		-- conditional actions
		if (rule.if_exists_then_map_to) then
			action_type = RULE_ACTION_IF_EXISTS_THEN_MAP_TO
			action_str = rule.if_exists_then_map_to
		elseif (rule.if_exists_then_replace_by) then
			action_type = RULE_ACTION_IF_EXISTS_THEN_REPLACE_BY
			action_str = rule.if_exists_then_replace_by
		else
			print("No action!")
			action_type = 0;
		end
	end

	if (rule.actions) then
		action_type = RULE_ACTION_CONDITIONAL_ACTIONS
		rule_list_link = get_rule_tree_offset_for_rule_list(rule.actions, false)
	elseif (rule.rules) then
		action_type = RULE_ACTION_SUBTREE
		rule_list_link = get_rule_tree_offset_for_rule_list(rule.rules, true)
	end

	-- Aux.conditions. these can be used in conditional actions.
	local condition_type = 0
	local condition_str = 0
	if (rule.if_active_exec_policy_is) then
		condition_type = RULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS
		condition_str = rule.if_active_exec_policy_is
	elseif (rule.if_redirect_ignore_is_active) then
		condition_type = RULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE
		condition_str = rule.if_redirect_ignore_is_active
	elseif (rule.if_redirect_force_is_active) then
		condition_type = RULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE
		condition_str = rule.if_redirect_force_is_active
	elseif (rule.if_env_var_is_not_empty) then
		condition_type = RULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY
		condition_str = rule.if_env_var_is_not_empty
	elseif (rule.if_env_var_is_empty) then
		condition_type = RULE_CONDITION_IF_ENV_VAR_IS_EMPTY
		condition_str = rule.if_env_var_is_empty
	end

	-- Selectors. 
	local selector_type = 0
	local selector = nil
	if (rule.dir) then
		selector_type = RULE_SELECTOR_DIR
		selector = rule.dir
	elseif (rule.prefix) then
		selector_type = RULE_SELECTOR_PREFIX
		selector = rule.prefix
	elseif (rule.path) then
		selector_type = RULE_SELECTOR_PATH
		selector = rule.path
	end

	-- flags:
	local flags = 0
	if (rule.readonly) then
		flags = flags + RULE_FLAGS_READONLY
	end
	if (rule.protection == readonly_fs_if_not_root) then
		flags = flags + RULE_FLAGS_READONLY_FS_IF_NOT_ROOT
	elseif (rule.protection == readonly_fs_always) then
		flags = flags + RULE_FLAGS_READONLY_FS_ALWAYS
	end

	if (rule.force_orig_path) then
		flags = flags + RULE_FLAGS_FORCE_ORIG_PATH
	end
	if (rule.custom_map_funct or rule.actions) then
		flags = flags + RULE_FLAGS_CALL_TRANSLATE_FOR_ALL
	end

	if (rule.custom_map_funct) then
		if (rule.custom_map_funct == sb2_procfs_mapper) then
			action_type = RULE_ACTION_PROCFS
			action_str = nil
		else
			-- user-provided custom mapping functions
			-- are not supported by C mapping engine,
			-- only way to support them is to fallback to Lua.
			action_type = RULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE
			action_str = nil
		end
	end

	-- FIXME: func_name is a Lua-style pattern, which isn't usable
	--	in C code. It should be converted to a func_class (which
	--	should be a bitmask, and classes should be defined for
	--	each interface function in interface.master). To Be Implemented.
	local func_class = 0;
	if (rule.func_name) then
		-- FIXME: 
		action_type = RULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE
		action_str = nil
		-- non-zero func_class causes fallback to Lua mapping. FIXME.
		func_class = 0;
	end

	local rule_offs = ruletree.add_rule_to_ruletree(name,
			selector_type, selector, action_type, action_str,
			condition_type, condition_str,
			rule_list_link, flags, rule.binary_name,
			func_class, rule.exec_policy_name)
	print (rule_offs,"adding selector_type=",selector_type," selector=",selector,
		"action_type=",action_type, " action_str=",action_str,
		"condition_type=",condition_type, " condition_str=",condition_str)
	return rule_offs
end

function add_list_of_rules(rules, node_type_is_ordinary_rule)
        local n

	print("-- add_list_of_rules:")

	local num_rules = table.maxn(rules)
	local rule_list_index = 0

	if num_rules > 0 then
		rule_list_index = ruletree.objectlist_create(num_rules)

		for n=1,table.maxn(rules) do
			local rule = rules[n]
			local new_rule_index

			new_rule_index = add_one_rule_to_rule_tree(rule, node_type_is_ordinary_rule)
			ruletree.objectlist_set(rule_list_index, n-1, new_rule_index)
		end
		print("-- Added to rule db: ",table.maxn(rules),"rules, idx=", rule_list_index)
	end
	return rule_list_index
end

ruletree.attach_ruletree()

local forced_modename = sb.get_forced_mapmode()
if forced_modename then
	print("forced_modename = "..forced_modename)
else
	print("forced_modename = nil")
end
print("sbox_mapmode = "..sbox_mapmode)
print("active_mapmode = "..active_mapmode)

local ri
ri = add_list_of_rules(fs_mapping_rules, true) -- add ordinary (forward) rules
print("-- Added ruleset fwd rules")
ruletree.set_ruletree_fsrules("rules", ri)


ri = add_list_of_rules(reverse_fs_mapping_rules, true) -- add reverse  rules
print("-- Added ruleset rev.rules")
ruletree.set_ruletree_fsrules("rev_rules", ri)

