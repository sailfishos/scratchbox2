-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

local forced_modename = sb.get_forced_mapmode()

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

	-- Returns exec_policy, path, readonly_flag
	if (mapped) then
		ret_path = mapped
	end
	return nil, ret_path, false
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
	-- (version 19 is in intermediate version;
	--  several interface changes will follow)
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
	local current_rule_interface_version = "19"

	do_file(rule_file_path)

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

function sbox_execute_replace_rule(path, replacement, rule)
	local ret = nil

	if (debug_messages_enabled) then
		sb.log("debug", string.format("replace:%s:%s", path, replacement))
	end
	if (rule.dir) then
		if ((rule.dir ~= "") and
		    (isprefix(rule.dir, path))) then
			ret = replacement .. string.sub(path, string.len(rule.dir)+1)
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replaced (dir) => %s", ret))
			end
		else
			ret = ""
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replacement failed (short path?)"))
			end
		end
	elseif (rule.prefix) then
		if ((rule.prefix ~= "") and
		    (isprefix(rule.prefix, path))) then
			ret = replacement .. string.sub(path, string.len(rule.prefix)+1)
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replaced (prefix) => %s", ret))
			end
		else
			ret = ""
			if (debug_messages_enabled) then
				sb.log("debug", string.format("replacement failed (short path?)"))
			end
		end
	elseif (rule.path) then
		-- "path" may be shorter than prefix during path resolution
		if (rule.path == path) then
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

-- returns exec_policy, path and readonly_flag
function sbox_execute_conditional_actions(binary_name,
		func_name, rp, path, rule)
	local actions = rule.actions
	local ret_exec_policy = nil

	local a
	for a = 1, table.maxn(actions) do
		if (debug_messages_enabled) then
			sb.log("debug", string.format("try %d", a))
		end

		-- each member in the "actions" array is a 
		-- candidate for the rule which will be applied
		local rule_cand = actions[a]

		local ret_ro = false
		if (rule_cand.readonly) then
			ret_ro = rule_cand.readonly
		end

		if (rule_cand.if_exists_then_map_to or
		    rule_cand.if_exists_then_replace_by) then
			-- conditional destinations: build a path to
			-- "tmp_dest", and if that destination exists, use that path.
			local tmp_dest = nil
			if (rule_cand.if_exists_then_map_to) then
				tmp_dest = rule_cand.if_exists_then_map_to .. path
			else
				tmp_dest = sbox_execute_replace_rule(path,
					rule_cand.if_exists_then_replace_by, rule)
			end
			if (sb.path_exists(tmp_dest)) then
				if (debug_messages_enabled) then
					sb.log("debug", string.format(
						"target exists: => %s", tmp_dest))
				end
				if (rule_cand.exec_policy ~= nil) then
					ret_exec_policy = rule_cand.exec_policy
				end
				return ret_exec_policy, tmp_dest, ret_ro
			end
		elseif (rule_cand.if_active_exec_policy_is) then
			local ep = get_active_exec_policy()

			if (ep ~= nil and ep.name == rule_cand.if_active_exec_policy_is) then
				return sbox_execute_rule(binary_name,
					 func_name, rp, path, rule_cand)
			end
		elseif (rule_cand.if_redirect_ignore_is_active) then
			if (sb.test_redirect_ignore(
				rule_cand.if_redirect_ignore_is_active)) then

				return sbox_execute_rule(binary_name,
					 func_name, rp, path, rule_cand)
			end
		else
			-- there MUST BE unconditional actions:
			if (rule_cand.use_orig_path 
			    or rule_cand.map_to or rule_cand.replace_by) then
				return sbox_execute_rule(binary_name,
					 func_name, rp, path, rule_cand)
			else
				sb.log("error", string.format(
					"error in rule: no valid conditional actions for '%s'",
					path))
			end
		end
	end

	-- no valid action found. This should not happen.
	sb.log("error", string.format("mapping rule for '%s': execution of conditional actions failed", path))

	return ret_exec_policy, path, false
end

-- returns exec_policy, path and readonly_flag
function sbox_execute_rule(binary_name, func_name, rp, path, rule)
	local ret_exec_policy = nil
	local ret_path = nil
	local ret_ro = false
	local rule_name

	if (rule.readonly ~= nil) then
		ret_ro = rule.readonly
	end
	if (rule.exec_policy ~= nil) then
		ret_exec_policy = rule.exec_policy
	end
	if (rule.use_orig_path) then
		ret_path = path
	elseif (rule.actions) then
		ret_exec_policy, ret_path, ret_ro = 
			sbox_execute_conditional_actions(binary_name,
				func_name, rp, path, rule)
	elseif (rule.map_to) then
		if (rule.map_to == "/") then
			ret_path = path
		else
			ret_path = rule.map_to .. path
		end
	elseif (rule.replace_by) then
		ret_path = sbox_execute_replace_rule(path, rule.replace_by, rule)
	else
		ret_path = path
		if (rule.name) then
			rule_name = rule.name
		else
			rule_name = "(no name)"
		end
		sb.log("error", string.format("mapping rule '%s' does not "..
			"have any valid actions, path=%s", rule_name, path))
	end
	
	return ret_exec_policy, ret_path, ret_ro
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
--   4. "readonly" flag
function sbox_translate_path(rule, binary_name, func_name, path)
	local ret = path
	local rp = path
	local readonly_flag = false
	local exec_policy = nil

	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find a match at all: %s(%s)", func_name, path))
		return nil, nil, path, readonly_flag
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
		exec_policy, ret, readonly_flag = rule.custom_map_funct(
			binary_name, func_name, rp, path, rule)
		if (rule.readonly ~= nil) then
			readonly_flag = rule.readonly
		end
	else
		exec_policy, ret, readonly_flag = sbox_execute_rule(
			binary_name, func_name, rp, path, rule)
	end

	return rule, exec_policy, ret, readonly_flag
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
-- returns "rule", "rule_found", "min_path_len", "call_translate_for_all"
-- ("call_translate_for_all" is a flag which controls optimizations in
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

		return nil, false, 0, false
	end

	rule, min_path_len = find_rule(chain, func_name, full_path)
	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find rule for: %s(%s)", func_name, full_path))
		return nil, false, 0, false
	end

	local call_translate_for_all = false
	if (rule.custom_map_funct) then
		call_translate_for_all = true
	end

	return rule, true, min_path_len, call_translate_for_all
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
-- returns "orig_path"
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

		return nil
	end

	rule, min_path_len = find_rule(chain, func_name, full_path)
	if (not rule) then
		-- not even a default rule found
		sb.log("info", string.format("Unable to find REVERSE rule for: %s(%s)", func_name, full_path))
		return nil
	end

	local rule2, exec_policy2, orig_path, ro2
	rule2, exec_policy2, orig_path, ro2 = sbox_translate_path(rule, 
		binary_name, func_name, full_path)

	return orig_path
end

