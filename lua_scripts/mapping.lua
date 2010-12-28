-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

local forced_modename = sb.get_forced_mapmode()

-- These must match the flag definitions in mapping.h:
local RULE_FLAGS_READONLY = 1
local RULE_FLAGS_CALL_TRANSLATE_FOR_ALL = 2
local RULE_FLAGS_FORCE_ORIG_PATH = 4

-- rule_file_path and rev_rule_file_path are global varibales
if forced_modename == nil then
	rule_file_path = session_dir .. "/rules/Default.lua"
	rev_rule_file_path = session_dir .. "/rev_rules/Default.lua"
	active_mapmode = sbox_mapmode
else
	rule_file_path = session_dir .. "/rules/" .. forced_modename .. ".lua"
	rev_rule_file_path = session_dir .. "/rev_rules/" .. forced_modename .. ".lua"
	active_mapmode = forced_modename
end

function basename(path)
	if (path == "/") then
		return "/"
	else
		return string.match(path, "[^/]*$")
	end
end

function dirname(path)
	local dir

	if (path == "/") then
		return "/"
	end
	dir = string.match(path, ".*/")
	if (dir == nil) then
		return "."
	end

	if (dir == "/") then return dir end

	-- chop off the trailing /
	if (string.sub(dir, string.len(dir)) == "/") then
		dir = string.sub(dir, 1, string.len(dir) - 1)
	end
	return dir 
end

-- isprefix(a, b) is now implemented in C; used to be:
--    function isprefix(a, b)
--	if (not a or not b) then return false end
--	return string.sub(b, 1, string.len(a)) == a
--    end
isprefix = sb.isprefix

function sb2_procfs_mapper(binary_name, func_name, rp, path, rule)
	local ret_path = path;

	if (debug_messages_enabled) then
		sb.log("debug", "sb2_procfs_mapper "..path.." : "..rp)
	end

	local mapped = sb.procfs_mapping_request(path)

	-- Returns exec_policy, path, flags
	if (mapped) then
		ret_path = mapped
	end
	return nil, ret_path, 0
end

-- all_exec_policies is a table, defined by the mapping rule file
all_exec_policies = nil

-- return the exec policy used for this process
--
local active_exec_policy_checked = false
local active_exec_policy_ptr = nil

function get_active_exec_policy()
	if (active_exec_policy_checked == false) then
		local ep_name = sb.get_active_exec_policy_name()

		if (ep_name and all_exec_policies ~= nil) then
			-- Name of it is known, try to find the object itself
			for i = 1, table.maxn(all_exec_policies) do
				if all_exec_policies[i].name == ep_name then
					active_exec_policy_ptr = all_exec_policies[i]
					break
				end
			end
			if (debug_messages_enabled) then
				if active_exec_policy_ptr then
					sb.log("debug", "Found active Exec policy "..ep_name)
				else
					sb.log("debug", "FAILED to find active Exec policy "..ep_name)
				end
			end
		else
			-- Don't know what exec policy is active
			if (debug_messages_enabled) then
				sb.log("debug", "Unknown active Exec policy")
			end
		end

		active_exec_policy_checked = true
	end
	return active_exec_policy_ptr
end

-- Override mapping rules from override_nomap table
--  Each entry from table override_nomap should specify path that shouldn't be remapped
--  Entries from this table will be searched before all other rules
function override_export_chains()
	if (not override_nomap) then
		return export_chains
	end

	override_chain = {
		next_chain = export_chains[1],
		binary = nil,
		rules = { },
	}

	for i, key in pairs(override_nomap) do
		if (key) then
			table.insert(override_chain.rules, {path = key, use_orig_path = true})
		end
	end

	local new_chain = { override_chain, }
	for i, key in pairs(export_chains) do
		table.insert(new_chain, key)
	end
	return new_chain
end

-- Load mode-specific rules.
-- A mode file must define three variables:
--  1. rule_file_interface_version (string) is checked and must match,
--     mismatch is a fatal error.
--  2. export_chains (array) contains mapping rule chains; this array
--     is searched sequentially with the original (unmapped) path as the key
--  3. exec_policy_chains (array) contains default execution policies;
--     real path (mapped path) is used as the key. A default exec_policy
--     must be present.
-- Additionally, following variables may be modified:
-- "enable_cross_gcc_toolchain" (default=true): All special processing
--     for the gcc-related tools (gcc,as,ld,..) will be disabled if set
--     to false.
--
function load_and_check_rules()

	-- initialize global variables:
	rule_file_interface_version = nil
	export_chains = {}
	exec_policy_chains = {}

	tools = tools_root
	if (not tools) then
		tools = "/"
	end

	-- INTERFACE VERSION between this file, the
	-- exec mapping code (argvenp.lua) and the
	-- rule files:
	--
	-- Version 25:
	-- - CPU Transparency is now defined by three variables:
	--    - sbox_cputransparency_method is the complete string,
	--      containing both the command and options. This is
	--	defined by the '-c' option of sb2-init.
	--    - sbox_cputransparency_cmd is path to the binary,
	--    - sbox_cputransparency_args contains optional
	--	arguments.
	--   Previously only sbox_cputransparency_method was
	--   available.
	-- Version 24:
	-- - Added support for gconv_path for native applications
	-- Version 23:
	-- - LD_LIBRARY_PATH and LD_PRELOAD are now always
	--   set by argvenvp.lua => the exec_policies must
	--   have proper policies for these (otherwise "fakeroot"
	--   may not perform as expected).
	-- - Note the by default the real LD_LIBRARY_PATH and
	--   LD_PRELOAD that will be used the real exec is now
	--   different than what the user will see in these
	--   variables!
	-- Version 22:
	-- - interface to custom_map_func was modified again:
	--   Last return value is now a bitmask (was a boolean)
	-- Version 21: native_app_message_catalog_prefix has
	-- been removed (it was an exec policy attribute.)
	-- Version 20 changed "script_interpreter_rule" field in
	-- exec policies to "script_interpreter_rules"; find_rule()
	-- is now used to select the rule (there may be more than one!)
	-- (version 19 was an intermediate version)
	-- - added "all_exec_policies" list to all
	--   mapping modes
	-- Differences between version 17 and 18:
	-- - added sb2_procfs_mapper()
	-- Differences between version 16 and 17:
	-- - Added support for hierarcic rules (i.e. rule
	--   trees. 16 supports only linear rule lists)
	-- Differences between version 15 and 16:
	-- - "match" rules are not supported anymore
	-- - interface to custom_map_func was modified:
	--   "work_dir" was removed, name changed to "custom_map_funct",
	--   and such functions are now expected to return 3 values
	--   (previously only one was expected)
	-- - variables "esc_tools_root" and "esc_target_root"
	--   were removed
	local current_rule_interface_version = "25"

	do_file(rule_file_path)
	export_chains = override_export_chains()

	-- fail and die if interface version is incorrect
	if (rule_file_interface_version == nil) or 
           (type(rule_file_interface_version) ~= "string") then
		sb.log("error", string.format(
			"Fatal: Rule file interface version check failed: "..
			"No version information in %s",
			rule_file_path))
		os.exit(99)
	end
	if rule_file_interface_version ~= current_rule_interface_version then
		sb.log("error", string.format(
			"Fatal: Rule file interface version check failed: "..
			"got %s, expected %s", rule_file_interface_version,
			current_rule_interface_version))
		os.exit(99)
	end

	-- export_chains variable contains now the mapping rule chains
	-- from the chunk
	for i = 1,table.maxn(export_chains) do
		-- fill in the default values
		if (not export_chains[i].rules) then
			export_chains[i].rules = {}
		end
		-- loop through the rules
		for r = 1, table.maxn(export_chains[i].rules) do
			export_chains[i].rules[r].lua_script = filename
			if (export_chains[i].binary) then
				export_chains[i].rules[r].binary_name = export_chains[i].binary
			else
				export_chains[i].rules[r].binary_name = "nil"
			end
			if (export_chains[i].rules[r].name == nil) then
				export_chains[i].rules[r].name = 
					string.format("rule:%d.%d",
						i, r)
			end
		end
		export_chains[i].lua_script = filename
		table.insert(active_mode_mapping_rule_chains, export_chains[i])
	end

	-- Handle exec_policy_chains variable from the chunk
	for i = 1,table.maxn(exec_policy_chains) do
		table.insert(active_mode_exec_policy_chains,
			exec_policy_chains[i])
	end
end

target_root = sbox_target_root
if (not target_root or target_root == "") then
	target_root = "/"
end

tools_root = sbox_tools_root
if (tools_root == "") then
	tools_root = nil
end

enable_cross_gcc_toolchain = true

active_mode_mapping_rule_chains = {}
active_mode_exec_policy_chains = {}

load_and_check_rules()

-- load reverse mapping rules, if those have been created
-- (the file does not exist during the very first round here)
reverse_chains = nil
if (sb.path_exists(rev_rule_file_path)) then
	sb.log("debug", "Loading reverse rules")
	do_file(rev_rule_file_path)
end
if (debug_messages_enabled) then
	if reverse_chains ~= nil then
		sb.log("debug", "Loaded reverse rules")
	else
		sb.log("debug", "No reverse rules")
	end
end

function sbox_execute_replace_rule(path, replacement, rule_selector)
	local ret = nil

	if (debug_messages_enabled) then
		sb.log("debug", string.format("replace:%s:%s", path, replacement))
	end
	if (rule_selector.dir) then
		if ((rule_selector.dir ~= "") and
		    (isprefix(rule_selector.dir, path))) then
			ret = replacement .. string.sub(path, string.len(rule_selector.dir)+1)
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replaced (dir) => %s", ret))
			end
		else
			ret = ""
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replacement failed (short path?)"))
			end
		end
	elseif (rule_selector.prefix) then
		if ((rule_selector.prefix ~= "") and
		    (isprefix(rule_selector.prefix, path))) then
			ret = replacement .. string.sub(path, string.len(rule_selector.prefix)+1)
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replaced (prefix) => %s", ret))
			end
		else
			ret = ""
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replacement failed (short path?)"))
			end
		end
	elseif (rule_selector.path) then
		-- "path" may be shorter than prefix during path resolution
		if (rule_selector.path == path) then
			ret = replacement
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replaced (path) => %s", ret))
			end
		else
			ret = ""
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replacement failed (short path?)"))
			end
		end
	else
		sb.log("error", "error in rule: can't replace without 'prefix' or 'path'")
		ret = path
	end

	return ret
end

function rule_logging(rule, path)
	if (rule.log_level) then
		if (rule.log_message) then
			sb.log(rule.log_level, string.format("%s (%s)",
				rule.log_message, path))
		else
			-- default message = log path
			sb.log(rule.log_level, string.format("path=(%s)", path))
		end
	end
end


-- returns exec_policy, path, flags
function sbox_execute_conditional_actions(binary_name,
		func_name, rp, path, rule_selector)
	local actions = rule_selector.actions
	local ret_exec_policy = nil

	local a
	for a = 1, table.maxn(actions) do
		if (debug_messages_enabled) then
			sb.log("debug", string.format("try %d", a))
		end

		-- each member in the "actions" array is a 
		-- candidate for the rule which will be applied
		local action_candidate = actions[a]

		local ret_flags = 0
		if (action_candidate.readonly) then
			ret_flags = RULE_FLAGS_READONLY
		end

		if (action_candidate.if_exists_then_map_to or
		    action_candidate.if_exists_then_replace_by) then
			-- conditional destinations: build a path to
			-- "tmp_dest", and if that destination exists, use that path.
			local tmp_dest = nil
			if (action_candidate.if_exists_then_map_to) then
				tmp_dest = action_candidate.if_exists_then_map_to .. path
			else
				tmp_dest = sbox_execute_replace_rule(path,
					action_candidate.if_exists_then_replace_by, rule_selector)
			end
			if (sb.path_exists(tmp_dest)) then
				if (debug_messages_enabled) then
					sb.log("debug", string.format(
						"target exists: => %s", tmp_dest))
				end
				if (action_candidate.exec_policy ~= nil) then
					ret_exec_policy = action_candidate.exec_policy
				end
				rule_logging(action_candidate, path)
				return ret_exec_policy, tmp_dest, ret_flags
			end
		elseif (action_candidate.if_active_exec_policy_is) then
			local ep = get_active_exec_policy()

			if (ep ~= nil and ep.name == action_candidate.if_active_exec_policy_is) then
				if (debug_messages_enabled) then
					sb.log("debug", string.format(
						"selected by exec_policy %s",
						ep.name))
				end
				rule_logging(action_candidate, path)
				return sbox_execute_rule(binary_name,
					func_name, rp, path,
					rule_selector, action_candidate)
			end
		elseif (action_candidate.if_redirect_ignore_is_active) then
			if (sb.test_if_listed_in_envvar(
				action_candidate.if_redirect_ignore_is_active,
				"SBOX_REDIRECT_IGNORE")) then

				if (debug_messages_enabled) then
					sb.log("debug", "selected; redirect ignore is active")
				end
				rule_logging(action_candidate, path)
				return sbox_execute_rule(binary_name,
					func_name, rp, path,
					rule_selector, action_candidate)
			end
		elseif (action_candidate.if_redirect_force_is_active) then
			if (sb.test_if_listed_in_envvar(
				action_candidate.if_redirect_force_is_active,
				"SBOX_REDIRECT_FORCE")) then

				if (debug_messages_enabled) then
					sb.log("debug", "selected; redirect force is active")
				end
				rule_logging(action_candidate, path)
				return sbox_execute_rule(binary_name,
					func_name, rp, path,
					rule_selector, action_candidate)
			end
		else
			-- there MUST BE unconditional actions:
			if (action_candidate.use_orig_path
			    or action_candidate.force_orig_path 
			    or action_candidate.map_to
			    or action_candidate.replace_by) then

				if (debug_messages_enabled) then
					sb.log("debug", "using default (unconditional) rule")
				end
				rule_logging(action_candidate, path)
				return sbox_execute_rule(binary_name,
					func_name, rp, path,
					rule_selector, action_candidate)
			else
				sb.log("error", string.format(
					"error in rule: no valid conditional actions for '%s'",
					path))
			end
		end
	end

	-- no valid action found. This should not happen.
	sb.log("error", string.format("mapping rule for '%s': execution of conditional actions failed", path))

	return ret_exec_policy, path, 0
end

-- returns exec_policy, path and readonly_flag
function sbox_execute_rule(binary_name, func_name, rp, path,
	rule_selector, rule_conditions_and_actions)

	local ret_exec_policy = nil
	local ret_path = nil
	local ret_flags = 0
	local rule_name

	if (rule_conditions_and_actions.readonly) then
		ret_flags = RULE_FLAGS_READONLY
	end
	if (rule_conditions_and_actions.exec_policy ~= nil) then
		ret_exec_policy = rule_conditions_and_actions.exec_policy
	end
	if (rule_conditions_and_actions.use_orig_path) then
		ret_path = path
	elseif (rule_conditions_and_actions.actions) then
		ret_exec_policy, ret_path, ret_flags = 
			sbox_execute_conditional_actions(binary_name,
				func_name, rp, path, rule_selector)
	elseif (rule_conditions_and_actions.map_to) then
		if (rule_conditions_and_actions.map_to == "/") then
			ret_path = path
		else
			ret_path = rule_conditions_and_actions.map_to .. path
		end
	elseif (rule_conditions_and_actions.replace_by) then
		ret_path = sbox_execute_replace_rule(path, rule_conditions_and_actions.replace_by, rule_selector)
	elseif (rule_conditions_and_actions.force_orig_path) then
		ret_path = path
		ret_flags = ret_flags + RULE_FLAGS_FORCE_ORIG_PATH
	else
		ret_path = path
		if (rule_selector.name) then
			rule_name = rule_selector.name
		else
			rule_name = "(no name)"
		end
		local rule_selector_str = ""
		if rule_selector.dir then
			rule_selector_str = "(dir="..rule_selector.dir..")"
		elseif rule_selector.path then
			rule_selector_str = "(path="..rule_selector.path..")"
		elseif rule_selector.prefix then
			rule_selector_str = "(prefix="..rule_selector.prefix..")"
		end
		sb.log("error", string.format("mapping rule '%s' %s does not "..
			"have any valid actions, path=%s", rule_name, rule_selector_str, path))
	end
	
	return ret_exec_policy, ret_path, ret_flags
end

-- returns rule and min_path_len, minimum length which is needed for
-- successfull mapping.
function find_rule(chain, func, full_path)
	local i = 0
	local wrk = chain
	local min_path_len = 0
	if (debug_messages_enabled) then
		sb.log("noise", string.format("find_rule for (%s)", full_path))
	end
	while (wrk) do
		-- travel the chains and loop the rules in a chain
		for i = 1, table.maxn(wrk.rules) do
			local rule = wrk.rules[i]
			-- sb.test_path_match() is implemented in C (better
			-- performance). It returns <0 if full_path doesn't
			-- match, min.length otherwise
			min_path_len = sb.test_path_match(full_path,
				rule.dir, rule.prefix, rule.path)
			if min_path_len >= 0 then
				if (rule.chain) then
					-- if rule can be found from
					-- a subtree, return it,
					-- otherwise continue looping here.
					local s_rule
					local s_min_len
					s_rule, s_min_len = find_rule(
						rule.chain, func, full_path)
					if (s_rule ~= nil) then
						return s_rule, s_min_len
					end
					if (debug_messages_enabled) then
						sb.log("noise",
						  "rule not found from subtree")
					end
				else
					-- Path matches, test if other conditions are
					-- also OK:
					if ((not rule.func_name
						or string.match(func,
							 rule.func_name))) then
						if (debug_messages_enabled) then
							local rulename = rule.name
							if rulename == nil then
								rulename = string.format("#%d",i)
							end

							sb.log("noise", string.format(
							  "selected rule '%s'",
							  rulename))
						end
						return rule, min_path_len
					end
					rule = nil
				end
			end
		end
		wrk = wrk.next_chain
	end
	if (debug_messages_enabled) then
		sb.log("noise", string.format("rule not found"))
	end
	return nil, 0
end

-- sbox_translate_path is the function called from libsb2.so
-- preload library and the FUSE system for each path that needs
-- translating.
--
-- returns:
--   1. the rule used to perform the mapping
--   2. exec_policy
--   3. path (mapping result)
--   4. Flags (bitmask)
function sbox_translate_path(rule, binary_name, func_name, path)
	local ret = path
	local rp = path
	local ret_flags = 0
	local exec_policy = nil

	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find a match at all: %s(%s)", func_name, path))
		return nil, nil, path, ret_flags
	end

	if (debug_messages_enabled) then
		sb.log("noise", string.format("map:%s", path))
	end

	if (rule.log_level) then
		if (rule.log_message) then
			sb.log(rule.log_level, string.format("%s (%s)",
				rule.log_message, path))
		else
			-- default message = log path
			sb.log(rule.log_level, string.format("path=(%s)", path))
		end
	end

	if (rule.custom_map_funct ~= nil) then
		exec_policy, ret, ret_flags = rule.custom_map_funct(
			binary_name, func_name, rp, path, rule)
		if (rule.readonly) then
			ret_flags = RULE_FLAGS_READONLY
		end
	else
		exec_policy, ret, ret_flags = sbox_execute_rule(
			binary_name, func_name, rp, path, rule, rule)
	end

	return rule, exec_policy, ret, ret_flags
end

function find_chain(chains_table, binary_name)
	local n

	for n=1,table.maxn(chains_table) do
		if (not chains_table[n].noentry
		    and (not chains_table[n].binary
		   	 or binary_name == chains_table[n].binary)) then
				return(chains_table[n])
		end
	end
end

-- sbox_get_mapping_requirements is called from libsb2.so before
-- path resolution takes place. The primary purpose of this is to
-- determine where to start resolving symbolic links; shorter paths than
-- "min_path_len" should not be given to sbox_translate_path()
-- returns "rule", "rule_found", "min_path_len", "flags"
-- ("flags" may contain "call_translate_for_all", which
-- is a flag which controls optimizations in
-- the path resolution code)
function sbox_get_mapping_requirements(binary_name, func_name, full_path)
	-- loop through the chains, first match is used
	local min_path_len = 0
	local rule = nil
	local chain

	chain = find_chain(active_mode_mapping_rule_chains, binary_name)
	if (chain == nil) then
		sb.log("error", string.format("Unable to find chain for: %s(%s)",
			func_name, full_path))

		return nil, false, 0, 0
	end

	rule, min_path_len = find_rule(chain, func_name, full_path)
	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find rule for: %s(%s)", func_name, full_path))
		return nil, false, 0, 0
	end

	local ret_flags = 0
	if (rule.custom_map_funct or rule.actions) then
		ret_flags = RULE_FLAGS_CALL_TRANSLATE_FOR_ALL
	end

	return rule, true, min_path_len, ret_flags
end

--
-- Tries to find exec_policy for given binary using exec_policy_chains.
--
-- Returns: 1, exec_policy when exec_policy was found, otherwise
-- returns 0, nil.
--
-- Called from libsb2.so, too.
function sb_find_exec_policy(binaryname, mapped_file)
	local rule = nil
	local chain = nil

	chain = find_chain(active_mode_exec_policy_chains, binaryname)
	if chain ~= nil then
		sb.log("debug", "chain found, find rule for "..mapped_file)
		-- func_name == nil
		rule = find_rule(chain, nil, mapped_file)
	end
	if rule ~= nil then
		sb.log("debug", "rule found..")
		return 1, rule.exec_policy
	end
	return 0, nil
end

-- sbox_reverse_path is called from libsb2.so
-- returns "orig_path", "flags"
function sbox_reverse_path(binary_name, func_name, full_path)
	-- loop through the chains, first match is used
	local min_path_len = 0
	local rule = nil
	local chain = nil

	if (reverse_chains ~= nil) then
		chain = find_chain(reverse_chains, binary_name)
	end
	if (chain == nil) then
		-- reverse mapping is an optional feature,
		-- so it isn't really an error if the rule
		-- can't be found.
		sb.log("info", string.format("Unable to find REVERSE chain for: %s(%s)",
			func_name, full_path))

		return nil, 0
	end

	rule, min_path_len = find_rule(chain, func_name, full_path)
	if (not rule) then
		-- not even a default rule found
		sb.log("info", string.format("Unable to find REVERSE rule for: %s(%s)", func_name, full_path))
		return nil, 0
	end

	local rule2, exec_policy2, orig_path, flags2
	rule2, exec_policy2, orig_path, flags2 = sbox_translate_path(rule, 
		binary_name, func_name, full_path)

	return orig_path, flags2
end

