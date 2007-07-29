-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

tools_root = os.getenv("SBOX_TOOLS_ROOT")
if (not tools_root) then
	tools_root = "/scratchbox/sarge"
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


-- SBOX_REDIR_SCRIPTS environment variable controls where
-- we look for the scriptlets defining the path mappings

rsdir = os.getenv("SBOX_REDIR_SCRIPTS")
if (rsdir == nil) then
	rsdir = "/scratchbox/redir_scripts"
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

function read_mode_part(mode, part)
	filename = rsdir .. "/preload/" .. mode .. "/" .. part
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" .. err .. "\n")
	else
		f() -- execute the loaded chunk
		-- export_chains variable contains now the chains
		-- from the chunk
		for i = 1,table.maxn(export_chains) do
			-- fill in the default values
			if (not export_chains[i].binary) then
				export_chains[i].binary = ".*"
			end
			if (not export_chains[i].rules) then
				export_chains[i].rules = {}
			end
			-- loop through the rules
			for r = 1, table.maxn(export_chains[i].rules) do
				if (not export_chains[i].rules[r].func_name) then
					export_chains[i].rules[r].func_name = ".*"
				end
				if (not export_chains[i].rules[r].path) then
					-- this is an error, report and exit
					os.exit(1)
				end
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

-- sb.sb_getdirlisting is provided by lua_bindings.c
-- it returns a table listing all files in a directory
mm = sb.sb_getdirlisting(rsdir .. "/preload")
table.sort(mm);
for m = 1, table.maxn(mm) do
	local t = sb.sb_getdirlisting(rsdir .. "/preload/" .. mm[m])
	local i = 0
	local r = 0
	if (mm[m] ~= "." and mm[m] ~= "..") then
		table.sort(t)
		modes[mm[m]] = {}
		modes[mm[m]].chains = {}
		-- load the individual parts ($SBOX_REDIR_SCRIPTS/preload/[modename]/*.lua)
		for n = 1,table.maxn(t) do
			if (string.match(t[n], "%a*%.lua$")) then
				read_mode_part(mm[m], t[n])
			end
		end
	end
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


function sb_debug(msg)
	local logfile = os.getenv("SBOX_MAPPING_LOGFILE")
	local f
	local err
	if (not logfile) then return end
	f, err = io.open(logfile, "a+")
	if (not f) then return end
	f:write(msg .. "\n")
	io.close(f)
end

function isprefix(a, b)
	return string.sub(b, 1, string.len(a)) == a
end

function adjust_for_mapping_leakage(path)
	if (not path) then 
		return nil
	end
	-- print("path: " .. path)
	local tmp = sb.sb_readlink(path)
	if (not tmp) then
		-- not a symlink
		return path
	end

	if (sb.sb_decolonize_path(tmp) == sb.sb_decolonize_path(path)) then
		-- symlink refers to itself
		return path
	end

	-- check if the file pointed to by the symlink
	-- exists, if not, return path
	
	if (not sb.sb_file_exists(tmp)) then
		return path
	end
	-- make it an absolute path if it's not
	if (string.sub(tmp, 1, 1) ~= "/") then
		tmp = dirname(path) .. "/" .. tmp
	end
	-- decolonize it
	tmp = sb.sb_decolonize_path(tmp)
	--print(string.format("after decolonizing: %s\n", tmp))

	if (not isprefix(target_root, tmp)) then
		-- aha! tried to get out of there, now map it right back in
		--print("it tried to LEAK\n")
		return adjust_for_mapping_leakage(target_root .. tmp)
	else
		return adjust_for_mapping_leakage(tmp)
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
	"mknod",
	"mknodat",
	"mkfifo",
	"mkfifoat",
	"readlink",
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

function sbox_map_to(binary_name, func_name, work_dir, rp, path, rule)
	local ret = nil
	if (rule.map_to) then
		if (string.sub(rule.map_to, 1, 1) == "=") then
			--print(string.format("rp: %s\ntarget_root: %s", rp, target_root))
			ret = target_root .. string.sub(rule.map_to, 2) .. path
		elseif (string.sub(rule.map_to, 1, 1) == "-") then
			ret = string.match(path, rule.path .. "(.*)")
			ret = string.sub(rule.map_to, 2) .. ret
		else
			ret = rule.map_to .. path
		end
		if (should_adjust(func_name)) then
			return adjust_for_mapping_leakage(ret)
		else
			return ret
		end
	end
	-- if not mapping, check if we're within the 
	-- target_root and adjust for mapping leakage if so
	if (isprefix(target_root, path)) then
		if (should_adjust(func_name)) then
			return adjust_for_mapping_leakage(path)
		else
			return path
		end
	else
		return path
	end
end


function find_rule(chain, func, path)
	local i = 0
	local wrk = chain
	while (wrk) do
		-- travel the chains
		for i = 1, table.maxn(wrk.rules) do
			-- loop the rules in a chain
			if (string.match(func, wrk.rules[i].func_name) and
				string.match(path, wrk.rules[i].path)) then
				return wrk.rules[i]
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

	-- print(string.format("looping through chains: %s", chains[n].binary))
	rule = find_rule(chain, func_name, rp)
	if (not rule) then
		-- error, not even a default rule found
		sb_debug(string.format("Unable to find a match at all: [%s][%s][%s]", binary_name, func_name, path))
		return path
	end
	if (rule.custom_map_func ~= nil) then
		ret = rule.custom_map_func(binary_name, func_name, work_dir, rp, path, rules[n])
	else
		ret = sbox_map_to(binary_name, func_name, work_dir, rp, path, rule)
		if (debug) then
			sb_debug(string.format("[%s][%s|%s]:\n  %s(%s) -> (%s)", basename(rule.lua_script), rule.binary_name, binary_name, func_name, path, ret))
		end
	end
	return ret
end

-- sbox_translate_path is the function called from libsb2.so
-- preload library and the FUSE system for each path that needs 
-- translating
function sbox_translate_path(mapping_mode, binary_name, func_name, work_dir, path)
	--sb_debug(string.format("[%s]:", binary_name))
	--sb_debug(string.format("debug: [%s][%s][%s][%s]", binary_name, func_name, work_dir, path))

	-- loop through the chains, first match is used
	for n=1,table.maxn(modes[mapping_mode].chains) do
		if (not modes[mapping_mode].chains[n].noentry 
			and string.match(binary_name, modes[mapping_mode].chains[n].binary)) then
			return map_using_chain(modes[mapping_mode].chains[n], binary_name, func_name, work_dir, path)
		end
	end

	-- we should never ever get here, if we still do, don't do anything
	return path
end

