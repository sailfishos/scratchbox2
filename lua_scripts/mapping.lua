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

function read_mode_part(mode, part)
	filename = rsdir .. "/pathmaps/" .. mode .. "/" .. part
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" .. err .. "\n")
	else
		f() -- execute the loaded chunk
		-- export_chains variable contains now the chains
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
			end
			export_chains[i].lua_script = filename
			table.insert(modes[mode].chains, export_chains[i])
		end
	end
end

-- modes represent the different mapping modes supported.
-- Each mode contains its own set of chains.
-- The mode is passed from the libsb2.so to here in the first
-- argument to sbox_translate_path()
modes = {}

-- sb.getdirlisting is provided by lua_bindings.c
-- it returns a table listing all files in a directory
mm = sb.getdirlisting(rsdir .. "/pathmaps")
table.sort(mm);
for m = 1, table.maxn(mm) do
	local t = sb.getdirlisting(rsdir .. "/pathmaps/" .. mm[m])
	local i = 0
	local r = 0
	if (mm[m] ~= "." and mm[m] ~= "..") then
		table.sort(t)
		modes[mm[m]] = {}
		modes[mm[m]].chains = {}
		
		-- load the individual parts from
		-- ($SBOX_REDIR_SCRIPTS/preload/[modename]/*.lua)
		for n = 1,table.maxn(t) do
			if (string.match(t[n], "%a*%.lua$")) then
				read_mode_part(mm[m], t[n])
			end
		end
	end
end

function adjust_for_mapping_leakage(path, leakage_prefix)
	if (not path) then 
		return nil
	end

	if (not isprefix(leakage_prefix, path)) then
		-- The mapping result is not
		-- expected to be inside leakage_prefix.
		return path
	end

	local tmp = sb.readlink(path)
	if (not tmp) then
		-- not a symlink
		return path
	end

	-- make it an absolute path if it's not
	if (string.sub(tmp, 1, 1) ~= "/") then
		tmp = dirname(path) .. "/" .. tmp
	end

	if (sb.decolonize_path(tmp) == sb.decolonize_path(path)) then
		-- symlink refers to itself
		return path
	end

	tmp = sb.decolonize_path(tmp)

	if (not isprefix(leakage_prefix, tmp)) then
		-- aha! tried to get out of there, now map it right back in
		return adjust_for_mapping_leakage(leakage_prefix .. tmp, leakage_prefix)
	else
		return adjust_for_mapping_leakage(tmp, leakage_prefix)
	end
end

no_adjust_funcs = {
	"__lxstat",
	"__lxstat64",
	"__xmknod",
	"lchmod",
	"lchown",
	"lgetxattr",
	"llistxattr",
	"lremovexattr",
	"lsetxattr",
	"lstat",
	"lstat64",
	"lutimes",
	"readlink",
	"rename",
	"renameat",
	"symlink",
	"symlinkat",
	"unlink",
	"unlinkat"
}

function should_adjust(func_name)
	for i = 1, table.maxn(no_adjust_funcs) do
		if (no_adjust_funcs[i] == func_name) then
			return false
		end
	end
	return true
end

function sbox_execute_replace_rule(path, replacement, rule)
	local ret = nil

	sb.log("debug", string.format("replace:%s:%s", path, replacement))
	if (rule.prefix) then
		ret = replacement .. string.sub(path, string.len(rule.prefix)+1)
		sb.log("debug", string.format("replaced (prefix) => %s", ret))
	elseif (rule.path) then
		ret = replacement
		sb.log("debug", string.format("replaced (path) => %s", ret))
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
		sb.log("debug", string.format("try %d", a))

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
				sb.log("debug", string.format("target exists: => %s", tmp_dest))
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

-- returns path and readonly_flag
function sbox_execute_rule(binary_name, func_name, work_dir, rp, path, rule)
	local ret_path = nil
	local ret_ro = false
	if (rule.use_orig_path) then
		ret_path = path
	elseif (rule.actions) then
		ret_path, ret_ro = sbox_execute_conditional_actions(binary_name,
			func_name, work_dir, rp, path, rule)
	elseif (rule.map_to) then
		ret_path = rule.map_to .. path
	elseif (rule.replace_by) then
		ret_path = sbox_execute_replace_rule(path, rule.replace_by, rule)
	else
		ret_path = path
		sb.log("error", "mapping rule uses does not have any valid actions, path="..path)
	end
	
	if (should_adjust(func_name)) then
		if (isprefix(target_root, ret_path)) then
			ret_path = adjust_for_mapping_leakage(ret_path, target_root)
		elseif (isprefix(tools_root, ret_path)) then
			ret_path = adjust_for_mapping_leakage(ret_path, tools_root)
		end
	end

	return ret_path, ret_ro
end


function find_rule(chain, func, path)
	local i = 0
	local wrk = chain
	while (wrk) do
		-- travel the chains
		for i = 1, table.maxn(wrk.rules) do
			-- loop the rules in a chain
			if ((not wrk.rules[i].func_name 
				or func == wrk.rules[i].func_name)) then
				-- "prefix" rules:
				-- compare prefix (only if a non-zero prefix)
				if (wrk.rules[i].prefix and
				    (wrk.rules[i].prefix ~= "") and
				    (isprefix(wrk.rules[i].prefix, path))) then
					return wrk.rules[i]
				end
				-- "path" rules: (exact match)
				if (wrk.rules[i].path == path) then
					return wrk.rules[i]
				end
				-- "match" rules use a lua "regexp".
				-- these will be obsoleted, as this kind of rule
				-- is almost impossible to reverse (backward mapping
				-- is not possible as long as there are "match" rules)
				if (wrk.rules[i].match) then
					if (string.match(path, wrk.rules[i].match)) then
						return wrk.rules[i]
					end
				end
				-- FIXME: Syntax checking should be added:
				-- it should be tested that exactly one of
				-- "prefix","path" or "match" was present
			end
		end
		wrk = wrk.next_chain
	end
	return nil
end


function map_using_chain(chain, binary_name, func_name, work_dir, path)
	local ret = path
	local rp = path
	local rule = nil
	local readonly_flag = false

	rule = find_rule(chain, func_name, rp)
	if (not rule) then
		-- error, not even a default rule found
		sb.log("error", string.format("Unable to find a match at all: %s(%s)", func_name, path))
		return path, readonly_flag
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
	else
		ret = sbox_execute_rule(binary_name, func_name, work_dir, rp, path, rule)
		if (debug) then
			if(path == ret) then
				-- sb.log("debug", string.format("[%s][%s] %s(%s) [==]", basename(rule.lua_script), rule.binary_name, func_name, path))
			else
				-- sb.log("debug", string.format("[%s][%s] %s(%s) -> (%s)", basename(rule.lua_script), rule.binary_name, func_name, path, ret))
			end
		end
	end
	if (rule.readonly ~= nil) then
		readonly_flag = rule.readonly
	end
	return ret, readonly_flag
end

-- sbox_translate_path is the function called from libsb2.so
-- preload library and the FUSE system for each path that needs 
-- translating
-- returns path and the "readonly" flag
function sbox_translate_path(mapping_mode, binary_name, func_name, work_dir, path)
	-- loop through the chains, first match is used
	for n=1,table.maxn(modes[mapping_mode].chains) do
		if (not modes[mapping_mode].chains[n].noentry 
			and (not modes[mapping_mode].chains[n].binary
			or binary_name == modes[mapping_mode].chains[n].binary)) then
			return map_using_chain(modes[mapping_mode].chains[n], binary_name, func_name, work_dir, path)
		end
	end
	-- we should never ever get here, if we still do, don't do anything
	sb.log("error", string.format("[-][-] %s(%s) [MAPPING FAILED]",
		func_name, path))

	return path, false
end

