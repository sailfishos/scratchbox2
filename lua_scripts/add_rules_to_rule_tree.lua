-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed when SB2 session is created
-- (from init.lua) to load FS mapping rules to the rule
-- tree database.

-- Rule tree constants. These must match the #defines in <rule_tree.h>
local RULE_SELECTOR_PATH = 101
local RULE_SELECTOR_PREFIX = 102
local RULE_SELECTOR_DIR = 103

local RULE_ACTION_USE_ORIG_PATH = 201
local RULE_ACTION_FORCE_ORIG_PATH = 202
local RULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT = 203
local RULE_ACTION_MAP_TO = 210
local RULE_ACTION_REPLACE_BY = 211
local RULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR = 212
local RULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR = 213
local RULE_ACTION_SET_PATH = 214
local RULE_ACTION_CONDITIONAL_ACTIONS = 220
local RULE_ACTION_SUBTREE = 230
local RULE_ACTION_IF_EXISTS_THEN_MAP_TO = 245
local RULE_ACTION_IF_EXISTS_THEN_REPLACE_BY = 246
local RULE_ACTION_PROCFS = 250
local RULE_ACTION_UNION_DIR = 251
local RULE_ACTION_IF_EXISTS_IN = 252

local RULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS = 301
local RULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE = 302
local RULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE = 303
local RULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY = 304
local RULE_CONDITION_IF_ENV_VAR_IS_EMPTY = 305
local RULE_CONDITION_IF_EXISTS_IN = 306

-- and these  must match the flag definitions in mapping.h:
local RULE_FLAGS_READONLY = 1
local RULE_FLAGS_CALL_TRANSLATE_FOR_ALL = 2
local RULE_FLAGS_FORCE_ORIG_PATH = 4
local RULE_FLAGS_READONLY_FS_IF_NOT_ROOT = 8
local RULE_FLAGS_READONLY_FS_ALWAYS = 16
local RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT = 32

-- ================= Mapping rules =================

function get_rule_tree_offset_for_rule_list(rules, modename)
	if #rules < 1 then
		if debug_messages_enabled then
			print ("-- NO RULES!")
		end
		return 0
	elseif rules[1]._rule_tree_offset == nil then
		-- Not yet in the tree, add it.
		rules[1]._rule_tree_offset = add_list_of_rules(rules, modename)
	else
		if debug_messages_enabled then
			print ("get..Return existing at ", rules[1]._rule_tree_offset)
		end
	end
	return rules[1]._rule_tree_offset
end

function get_rule_tree_offset_for_union_dir_list(union_dir_list)
	if #union_dir_list < 1 then
		if debug_messages_enabled then
			print ("-- NO DIRS FOR UNION_DIR!")
		end
		return 0
	end

	local union_dir_rule_list_index = ruletree.objectlist_create(#union_dir_list)

	for n=1,table.maxn(union_dir_list) do
		local component_path = union_dir_list[n]
		local new_str_index = ruletree.new_string(component_path)

		ruletree.objectlist_set(union_dir_rule_list_index, n-1, new_str_index)
	end
	if debug_messages_enabled then
		print("-- Added union dir to rule db: ",table.maxn(union_dir_list),
			"rules, idx=", union_dir_rule_list_index)
	end
	return union_dir_rule_list_index
end

-- Add a rule to the rule tree, return rule offset in the file.
function add_one_rule_to_rule_tree(rule, modename)
	local action_type = 0
	local action_str = nil
	local name
	local rule_list_link = 0

	if (rule.name == nil) then
		name = ""
	else
		name = rule.name
	end

	if debug_messages_enabled then
		print(string.format("\t-- name=\"%s\",", name))
	end

	-- Actions
	-- Unconditional actions first
	if (rule.use_orig_path) then
		action_type = RULE_ACTION_USE_ORIG_PATH
	elseif (rule.force_orig_path) then
		action_type = RULE_ACTION_FORCE_ORIG_PATH
	elseif (rule.force_orig_path_unless_chroot) then
		action_type = RULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT
	elseif (rule.map_to) then
		action_type = RULE_ACTION_MAP_TO
		action_str = rule.map_to
	elseif (rule.replace_by) then
		action_type = RULE_ACTION_REPLACE_BY
		action_str = rule.replace_by
	elseif (rule.set_path) then
		action_type = RULE_ACTION_SET_PATH
		action_str = rule.set_path
	elseif (rule.map_to_value_of_env_var) then
		action_type = RULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR
		action_str = rule.map_to_value_of_env_var
	elseif (rule.replace_by_value_of_env_var) then
		action_type = RULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR
		action_str = rule.replace_by_value_of_env_var
	elseif (rule.union_dir) then
		action_type = RULE_ACTION_UNION_DIR
		rule_list_link = get_rule_tree_offset_for_union_dir_list(rule.union_dir)
	else
		-- conditional actions
		if (rule.if_exists_then_map_to) then
			action_type = RULE_ACTION_IF_EXISTS_THEN_MAP_TO
			action_str = rule.if_exists_then_map_to
		elseif (rule.if_exists_then_replace_by) then
			action_type = RULE_ACTION_IF_EXISTS_THEN_REPLACE_BY
			action_str = rule.if_exists_then_replace_by
		elseif (rule.if_exists_in) then
			action_type = RULE_ACTION_IF_EXISTS_IN
			action_str = rule.if_exists_in
		else
			if debug_messages_enabled then
				print("No action!")
			end
			action_type = 0;
		end
	end

	if (rule.actions) then
		action_type = RULE_ACTION_CONDITIONAL_ACTIONS
--		This call was the only one with node_type_is_ordinary_rule = "false"
		rule_list_link = get_rule_tree_offset_for_rule_list(rule.actions, modename)
	elseif (rule.rules) then
		action_type = RULE_ACTION_SUBTREE
		rule_list_link = get_rule_tree_offset_for_rule_list(rule.rules, modename)
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
	elseif (rule.if_exists_in) then
		condition_type = RULE_CONDITION_IF_EXISTS_IN
		condition_str = rule.if_exists_in
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

	if selector_type == 0 then
		if rule.optional_rule ~= true then
			-- TODO: This is warning, but should be changed to an error
			-- once the rule files have been converted.
			local msg = string.format(
				"Rule loader(%s): rule %s does not have a selector (dir,prefix or path), and is "..
				"not marked with 'optional_rule = true'\n", modename, name)
			sblib.log("warning", msg)
			-- Should be: io.stderr:write("Error:" .. msg)
		end
		-- return 0
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
	if (rule.force_orig_path_unless_chroot) then
		flags = flags + RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT
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
			-- are not supported by C mapping engine.
			io.stderr:write(string.format(
				"Error: Rule loader (%s): unsupported custom_map_funct in rule file\n",
				modename))
			return 0
		end
	end

	-- a classmask == bitmask
	local func_class = rule.func_class;

	local rule_offs = ruletree.add_rule_to_ruletree(name,
			selector_type, selector, action_type, action_str,
			condition_type, condition_str,
			rule_list_link, flags, rule.binary_name,
			func_class, rule.exec_policy_name)
	if debug_messages_enabled then
		print (rule_offs,"adding selector_type=",selector_type," selector=",selector,
			"action_type=",action_type, " action_str=",action_str,
			"condition_type=",condition_type, " condition_str=",condition_str,
			"func_class=",func_class)
	end
	if rule_offs == 0 then
		if rule.optional_rule ~= true then
			local s = selector
			if s == nil then
				s = ""
			end
			io.stderr:write(string.format(
				"Error: Rule loader(%s): Failed to insert rule to rule tree, name='%s' "..
				"selector='%s'\n", modename, name, s))
		end
	end
	return rule_offs
end

function add_list_of_rules(rules, modename)
        local n

	if debug_messages_enabled then
		print("-- add_list_of_rules:")
	end
	local rule_list_index = 0

	if rules ~= nil then
		local num_rules = table.maxn(rules)

		if num_rules > 0 then
			rule_list_index = ruletree.objectlist_create(num_rules)

			for n=1,table.maxn(rules) do
				local rule = rules[n]
				local new_rule_index

				new_rule_index = add_one_rule_to_rule_tree(rule, modename)
				ruletree.objectlist_set(rule_list_index, n-1, new_rule_index)
			end
			if debug_messages_enabled then
				print("-- Added to rule db: ",table.maxn(rules),"rules, idx=", rule_list_index)
			end
		end
	end
	return rule_list_index
end

-- ================= Exec rules =================

local valid_keywords_in_exec_policy = {
	name = "string",

	log_level = "string",
	log_message = "string",

	native_app_ld_library_path_prefix = "string",
	native_app_ld_library_path_suffix = "string",
	native_app_ld_preload_prefix = "string",

	native_app_ld_so = "string",
	native_app_ld_so_rpath_prefix = "string",
	native_app_ld_so_supports_argv0 = "boolean",
	native_app_ld_so_supports_nodefaultdirs = "boolean",
	native_app_ld_so_supports_rpath_prefix = "boolean",

	native_app_locale_path = "string",
	native_app_gconv_path = "string",

	exec_flags = "number",

	script_log_level = "string",
	script_log_message = "string",
	script_set_argv0_to_mapped_interpreter = "boolean",
	script_interpreter_rules = "MappingRules",
}

function add_to_exec_policy(modename_in_ruletree, ep_name, key, t, val)
	if t == "string" then
		ruletree.catalog_vset("exec_policy", modename_in_ruletree, ep_name,
			key, ruletree.new_string(val))
	elseif t == "boolean" then
		ruletree.catalog_vset("exec_policy", modename_in_ruletree, ep_name,
			key, ruletree.new_boolean(val))
	elseif t == "number" then
		ruletree.catalog_vset("exec_policy", modename_in_ruletree, ep_name,
			key, ruletree.new_uint32(val))
	else
		io.stderr:write(string.format(
			"exec policy loader: unsupported type %s (%s,%s,%s)\n",
			t, modename_in_ruletree, ep_name, key))
	end
end

function add_mapping_rules_to_exec_policy(modename_in_ruletree, ep_name, key, val)
	local ri = add_list_of_rules(val,  modename_in_ruletree)
	ruletree.catalog_vset("exec_policy", modename_in_ruletree, ep_name,
		key, ri)
end

function add_all_exec_policies(modename_in_ruletree)
        if (all_exec_policies ~= nil) then
                for i = 1, table.maxn(all_exec_policies) do
                        local ep_name = all_exec_policies[i].name
			if ep_name then
				sblib.log("debug", "Adding Exec policy "..ep_name)
				for key,val in pairs(all_exec_policies[i]) do
					local required_type = valid_keywords_in_exec_policy[key]
					local t = type(val)
					if required_type then
						if t == required_type then
							add_to_exec_policy(modename_in_ruletree, ep_name,
								key, t, val)
						elseif required_type == "MappingRules" and
							t == "table" then
							add_mapping_rules_to_exec_policy(
								modename_in_ruletree, ep_name,
								key, val)
						else
							io.stderr:write(string.format(
								"exec policy loader: Invalid type %s for keyword "..
								"%s in exec policy, expected %s (mode=%s, policy=%s)\n",
								t, key, required_type, modename_in_ruletree, ep_name))
						end
					else
						io.stderr:write(string.format(
							"exec policy loader: Invalid keyword"..
							" %s in exec policy (mode=%s, policy=%s)\n",
							key, modename_in_ruletree, ep_name))
					end
				end
			end
                end
        end
end

-- ================= Main =================

for m_index,m_name in pairs(all_modes) do
	local modename_in_ruletree = m_name

	local autorule_file_path = session_dir .. "/rules_auto/" .. m_name .. ".usr_bin.lua"
        local rule_file_path = session_dir .. "/rules/" .. m_name .. ".lua"
	local rev_rule_filename = session_dir .. "/rev_rules/" ..
                 m_name .. ".lua"
	local exec_rule_file_path = session_dir .. "/exec_rules/" ..
		 m_name .. ".lua"


	-- FS rulefile will set these:
        rule_file_interface_version = nil
        fs_mapping_rules = nil

	-- Exec rulefile will set:
        all_exec_policies = nil

        -- rulefiles expect to see this:
        active_mapmode = m_name

        -- Reload "constants", just to be sure:
        do_file(session_dir .. "/lua_scripts/rule_constants.lua")
        do_file(session_dir .. "/lua_scripts/exec_constants.lua")

	-- Main config file:
	do_file(session_dir .. "/share/scratchbox2/modes/"..modename_in_ruletree.."/config.lua")

	-- Load FS rules for this mode:
	do_file(autorule_file_path)
        do_file(rule_file_path)
        do_file(rev_rule_filename)

	-- Load exec policies:
	do_file(session_dir .. "/exec_config.lua")
	do_file(exec_rule_file_path)

	if debug_messages_enabled then
		print("sbox_mapmode = "..sbox_mapmode)
		print("active_mapmode = "..active_mapmode)
		print("modename_in_ruletree = "..modename_in_ruletree)
	end

	local ri
	ri = add_list_of_rules(fs_mapping_rules, m_name) -- add ordinary (forward) rules
	if debug_messages_enabled then
		print("-- Added ruleset fwd rules")
	end
	ruletree.catalog_set("fs_rules", modename_in_ruletree, ri)

	ri = add_list_of_rules(reverse_fs_mapping_rules, "reverse "..m_name) -- add reverse  rules
	if debug_messages_enabled then
		print("-- Added ruleset rev.rules")
	end
	ruletree.catalog_set("rev_rules", modename_in_ruletree, ri)

	add_all_exec_policies(modename_in_ruletree)
end
