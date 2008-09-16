-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

tools_root = os.getenv("SBOX_TOOLS_ROOT")
if (tools_root == "") then
	tools_root = nil
end



target_root = os.getenv("SBOX_TARGET_ROOT")
if (not target_root) then
	target_root = "/"
end

compiler_root = os.getenv("SBOX_COMPILER_ROOT")
if (not compiler_root) then
	compiler_root = "/usr"
end

debug = os.getenv("SBOX_MAPPING_DEBUG")

debug_messages_enabled = sb.debug_messages_enabled()

-- SBOX_LUA_SCRIPTS environment variable controls where
-- we look for the scriptlets defining the path mappings

rsdir = os.getenv("SBOX_LUA_SCRIPTS")
if (rsdir == nil) then
	rsdir = "/scratchbox/lua_scripts"
end

-- escape_string() prefixes the magic pattern matching
-- characters ^$()%.[]*+-?) with '%'
function escape_string(a)
	b = ""
	for i = 1, string.len(a) do
		c = string.sub(a, i, i)
		-- escape the magic chars
		if (c == "^" or
			c == "$" or
			c == "(" or
			c == ")" or
			c == "%" or
			c == "." or
			c == "[" or
			c == "]" or
			c == "*" or
			c == "+" or
			c == "-" or
			c == "?") then
			b = b .. "%"
		end
		b = b .. c
	end
	return b
end

function basename(path)
	if (path == "/") then
		return "/"
	else
		return string.match(path, "[^/]*$")
	end
end

function dirname(path)
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


function isprefix(a, b)
	if (not a or not b) then return false end
	return string.sub(b, 1, string.len(a)) == a
end

-- make versions of tools_root and target_root safe
-- to use in match() functions
if (tools_root) then
	esc_tools_root = escape_string(tools_root)
end

if (target_root) then
	esc_target_root = escape_string(target_root)
end

function load_and_execute_lua_script(filename)
	if (debug_messages_enabled) then
		sb.log("debug", string.format("Loading '%s'", filename))
	end
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" ..
			 err .. "\n")
		-- "error()" never returns
	end

	f() -- execute the loaded chunk
	if (debug_messages_enabled) then
		sb.log("debug", string.format("Loaded '%s'", filename))
	end
end

session_dir = os.getenv("SBOX_SESSION_DIR")

-- Load session-specific settings
function load_session_settings()
	load_and_execute_lua_script(session_dir .. "/sb2-session.conf")
	load_and_execute_lua_script(session_dir .. "/exec_config.lua")
end

-- Load mode-specific rules.
-- A mode file should define two variables:
--  1. export_chains (array) contains mapping rule chains; this array
--     is searched sequentially with the original (unmapped) path as the key
--  2. exec_policy_chains (array) contains default execution policies;
--     real path (mapped path) is used as the key.
--
function load_and_check_rules()

	export_chains = {}
	exec_policy_chains = {}

	load_and_execute_lua_script(session_dir .. "/rules.lua")

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

load_session_settings()

active_mode_mapping_rule_chains = {}
active_mode_exec_policy_chains = {}

load_and_check_rules()

function sbox_execute_replace_rule(path, replacement, rule)
	local ret = nil

	if (debug_messages_enabled) then
		sb.log("debug", string.format("replace:%s:%s", path, replacement))
	end
	if (rule.prefix) then
		-- "path" may be shorter than prefix during path resolution
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

-- returns path and readonly_flag
function sbox_execute_conditional_actions(binary_name,
		func_name, work_dir, rp, path, rule)
	local actions = rule.actions

	local a
	for a = 1, table.maxn(actions) do
		if (debug_messages_enabled) then
			sb.log("debug", string.format("try %d", a))
		end

		local ret_ro = false
		if (actions[a].readonly) then
			ret_ro = actions[a].readonly
		end

		-- first, if there are any unconditional actions:
		if (actions[a].use_orig_path) then
			return path, ret_ro
		elseif (actions[a].map_to) then
			return actions[a].map_to .. path, ret_ro
		end

		-- next try conditional destinations: build a path to
		-- "tmp_dest", and if that destination exists, use that path.
		local tmp_dest = nil
		if (actions[a].if_exists_then_map_to) then
			tmp_dest = actions[a].if_exists_then_map_to .. path
		elseif (actions[a].if_exists_then_replace_by) then
			tmp_dest = sbox_execute_replace_rule(path,
				actions[a].if_exists_then_replace_by, rule)
		end
		if (tmp_dest ~= nil) then
			if (sb.path_exists(tmp_dest)) then
				if (debug_messages_enabled) then
					sb.log("debug", string.format("target exists: => %s", tmp_dest))
				end
				return tmp_dest, ret_ro
			end
		else
			sb.log("error", string.format("error in rule: no valid conditional actions for '%s'", path))
		end
	end

	-- no valid action found. This should not happen.
	sb.log("error", string.format("mapping rule for '%s': execution of conditional actions failed", path))

	return path, false
end

-- returns exec_policy, path and readonly_flag
function sbox_execute_rule(binary_name, func_name, work_dir, rp, path, rule)
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
		-- FIXME: sbox_execute_conditional_actions should also
		-- be able to return exec_policy
		ret_path, ret_ro = sbox_execute_conditional_actions(binary_name,
			func_name, work_dir, rp, path, rule)
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
		-- travel the chains
		for i = 1, table.maxn(wrk.rules) do
			-- loop the rules in a chain
			if ((not wrk.rules[i].func_name 
				or string.match(func, wrk.rules[i].func_name))) then
				-- "prefix" rules:
				-- compare prefix (only if a non-zero prefix)
				if (wrk.rules[i].prefix and
				    (wrk.rules[i].prefix ~= "") and
				    (isprefix(wrk.rules[i].prefix, full_path))) then
					if (debug_messages_enabled) then
						sb.log("noise", string.format("selected prefix rule %d (%s)", i, wrk.rules[i].prefix))
					end
					min_path_len = string.len(wrk.rules[i].prefix)
					return wrk.rules[i], min_path_len
				end
				-- "path" rules: (exact match)
				if (wrk.rules[i].path == full_path) then
					if (debug_messages_enabled) then
						sb.log("noise", string.format("selected path rule %d (%s)", i, wrk.rules[i].path))
					end
					min_path_len = string.len(wrk.rules[i].path)
					return wrk.rules[i], min_path_len
				end
				-- "match" rules use a lua "regexp".
				-- these will be obsoleted, as this kind of rule
				-- is almost impossible to reverse (backward mapping
				-- is not possible as long as there are "match" rules)
				if (wrk.rules[i].match) then
					if (string.match(full_path, wrk.rules[i].match)) then
						if (debug_messages_enabled) then
							sb.log("noise", string.format("selected match rule %d (%s)", i, wrk.rules[i].match))
						end
						-- there is no easy and reliable
						-- way to determine min_path_len
						-- so leave it to zero
						return wrk.rules[i], min_path_len
					end
				end
				-- FIXME: Syntax checking should be added:
				-- it should be tested that exactly one of
				-- "prefix","path" or "match" was present
			end
		end
		wrk = wrk.next_chain
	end
	if (debug_messages_enabled) then
		sb.log("noise", string.format("rule not found"))
	end
	return nil, 0
end

-- returns the same values as sbox_translate_path
-- (rule,exec_policy,path,ro_flag):
function map_using_rule(rule, binary_name, func_name, work_dir, path)
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
		sb.log("noise", string.format("map:%s:%s", work_dir, path))
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

	if (rule.custom_map_func ~= nil) then
		ret = rule.custom_map_func(binary_name, func_name, work_dir, rp, path, rules[n])
		if (rule.readonly ~= nil) then
			readonly_flag = rule.readonly
		end
	else
		exec_policy, ret, readonly_flag = sbox_execute_rule(binary_name, func_name, work_dir, rp, path, rule)
	end

	return rule, exec_policy, ret, readonly_flag
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
function sbox_translate_path(rule, binary_name, func_name, work_dir, path)
	return map_using_rule(rule, binary_name, func_name, work_dir, path)
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
-- returns "rule", "rule_found", "min_path_len"
function sbox_get_mapping_requirements(binary_name, func_name, work_dir, full_path)
	-- loop through the chains, first match is used
	local min_path_len = 0
	local rule = nil
	local chain

	chain = find_chain(active_mode_mapping_rule_chains, binary_name)
	if (chain == nil) then
		sb.log("error", string.format("Unable to find chain for: %s(%s)",
			func_name, full_path))

		return nil, false, 0
	end

	rule, min_path_len = find_rule(chain, func_name, full_path)
	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find rule for: %s(%s)", func_name, full_path))
		return nil, false, 0
	end

	return rule, true, min_path_len
end

