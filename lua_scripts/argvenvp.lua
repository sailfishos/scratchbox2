-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Portion Copyright (c) 2008 Nokia Corporation.
-- (exec postprocessing code written by Lauri T. Aarnio at Nokia)
--
-- Licensed under MIT license

argvmods = {}

-- only map gcc & friends if a cross compiler has been defined
gcc_bindir = os.getenv("SBOX_CROSS_GCC_DIR")
if (gcc_bindir ~= nil and gcc_bindir ~= "") then
	-- this generates gcc related argv/envp manglings
	do_file(lua_scripts .. "/argvenvp_gcc.lua")
end

-- regular mangling rules go here
-- syntax is of the form:
--
-- rule = {
-- 	name = "binary-name",
-- 	add_head = {"list", "of", "args", "to", "prepend"},
-- 	add_tail = {"these", "are", "appended"},
-- 	remove = {"args", "to", "remove"},
-- 	new_filename = "exec-this-binary-instead",
-- 	disable_mapping = 1 -- set this to disable mappings
-- }
-- argvmods[rule.name] = rule
--
-- Environment modifications are not supported yet, except for disabling
-- mappings.
--
-- TODO:
--
-- * new_filename should probably be replaced by integrating argv/envp
--   mangling with the path mapping machinery.

dpkg_architecture = {
	name = "dpkg-architecture",
	remove = {"-f"}
}
argvmods[dpkg_architecture.name] = dpkg_architecture

-- ------------------------------------
-- Exec preprocessing.
-- function sb_execve_preprocess is called to decide WHAT FILE
-- should be started (see description of the algorithm in sb_exec.c)
-- (this also typically adds, deletes, or modifies arguments whenever needed)
-- 
-- returns: err, file, argc, argv, envc, envp
-- (zero as "err" means "OK")
function sbox_execve_preprocess(filename, argv, envp)
	local new_argv = {}
	local new_envp = {}
	local binaryname = string.match(filename, "[^/]+$")
	local new_filename = filename

	-- print(string.format("sbox_execve_preprocess(): %s\n", filename))
	
	new_envp = envp

	am = argvmods[binaryname]
	if (am and not am.remove) then am.remove = {} end
	if (am and not am.add_head) then am.add_head = {} end
	if (am and not am.add_tail) then am.add_tail = {} end

	if (am ~= nil) then
		-- head additions
		for i = 1, table.maxn(am.add_head) do
			table.insert(new_argv, am.add_head[i])
		end
	
		-- populate new_argv, skip those that are to be removed
		for i = 1, table.maxn(argv) do
			local match = 0
			for j = 1, table.maxn(am.remove) do
				if (argv[i] == am.remove[j]) then
					match = 1
				end
			end
			if (match == 0) then
				table.insert(new_argv, argv[i])
			end
		end
		
		-- tail additions
		for i = 1, table.maxn(am.add_tail) do
			table.insert(new_argv, am.add_tail[i])
		end

		if (am.new_filename) then
			-- print(string.format("changing to: %s\n", am.new_filename))
			new_filename = am.new_filename
			new_argv[1] = am.new_filename
		end
		if (am.disable_mapping) then
			table.insert(new_envp, "SBOX_DISABLE_MAPPING=1")
			table.insert(new_envp, "SBOX_DISABLE_ARGVENVP=1")
		end
	else
		new_argv = argv
	end
	return 0, new_filename, #new_argv, new_argv, #new_envp, new_envp
end

-- ------------------------------------
-- Exec postprocessing.
-- function sb_execve_postprocess is called to decide HOW the executable
-- should be started (see description of the algorithm in sb_exec.c)
-- 
-- returns: status, mapped_file, file, argc, argv, envc, envp
-- "status":
--    -1 = do not execute.
--    0 = argc&argv were updated, OK to execute with the new params
--    1 = ok to exec directly with orig.arguments

function sb_execve_postprocess_native_executable(rule, exec_policy,
	exec_type, mapped_file, filename, argv, envp)

	-- Native binary. See what we need to do with it...
	sb.log("debug", string.format("sb_execve_postprocess: Native binary"))
	
	sb.log("debug", "Rule: apply exec_policy")

	if (rule.prefix ~= nil) then
		sb.log("debug", string.format("rule.prefix=%s", rule.prefix))
	end
	if (rule.path ~= nil) then
		sb.log("debug", string.format("rule.path=%s", rule.path))
	end

	local new_argv = {}
	local new_envp = envp
	local new_filename = filename
	local new_mapped_file = mapped_file
	-- by default, copy argv from index 1 (refers to argv[0])
	local first_argv_element_to_copy = 1
	local updated_args = 0

	if (exec_policy.native_app_ld_so ~= nil) then
		-- we need to use ld.so for starting the binary, 
		-- instead of starting it directly:
		new_mapped_file = exec_policy.native_app_ld_so
		table.insert(new_argv, exec_policy.native_app_ld_so)

		if (exec_policy.native_app_ld_library_path ~= nil) then
			table.insert(new_argv, "--library-path")
			table.insert(new_argv, exec_policy.native_app_ld_library_path)
		end

		-- NOTE/WARNING: (FIXME, well, FIX-ld.so in fact!)
		-- Currently argv[0] will be lost, always.
		--
		-- There is no way to give argv[0] to ld.so when executing
		-- ls.so "from the command line", because ld.so will always
		-- use the pathname as argv[0] to main() of the loaded program.
		-- This should be fixed by fixing ld.so (which is part of
		-- glibc in Linux). To Be Implemented. 
		--
		-- Replace argv[0] by pathname:
		table.insert(new_argv, mapped_file)
		first_argv_element_to_copy = 2

		updated_args = 1
	elseif (exec_policy.native_app_ld_library_path ~= nil) then
		-- Start the binary with a nonstandard LD_LIBRARY_PATH
		local lib_path_found = 0
		for j = 1, table.maxn(new_envp) do
			if (string.match(new_envp[j], "^LD_LIBRARY_PATH=")) then
				new_envp[j] = exec_policy.native_app_ld_library_path
				sb.log("debug", string.format(
					"Replaced LD_LIBRARY_PATH=%s", 
					new_envp[j]))
				local lib_path_found = 1
			end
		end
		if (lib_path_found == 0) then
			table.insert(new_envp, 
				"LD_LIBRARY_PATH="..exec_policy.native_app_ld_library_path)
			sb.log("debug", "Added LD_LIBRARY_PATH")
		end
		updated_args = 1
	end
	
	if (updated_args == 1) then
		-- Add components from original argv[]
		local i
		for i = first_argv_element_to_copy, table.maxn(argv) do
			table.insert(new_argv, argv[i])
		end

		return 0, new_mapped_file, new_filename, #new_argv, new_argv, #new_envp, new_envp
	end

	-- else args not modified.
	return 1, mapped_file, filename, #argv, argv, #envp, envp
end

-- This is called from C:
function sb_execve_postprocess(rule, exec_policy, exec_type,
	mapped_file, filename, binaryname, argv, envp)

	-- First, if either rule or the exec policy is a string, something
	-- has failed during the previous steps or mapping has been disabled;
	-- in both cases postprocessing is not needed, exec must be allowed.
	if ((type(rule) == "string") or (type(exec_policy) == "string")) then
		local rs = ""
		local eps = ""
		if (type(rule) == "string") then
			rs = rule
		end
		if (type(exec_policy) == "string") then
			eps = exec_policy
		end

		sb.log("debug", "sb_execve_postprocess: "..rs..";"..eps..
			" (going to exec with orig.args)")
		return 1, mapped_file, filename, #argv, argv, #envp, envp
	end

	-- if exec_policy was not provided by the caller (i.e. not
	-- provided by the mapping rule), look up the policy from
	-- exec_policy_chains array.
	if (exec_policy == nil) then
		local mapping_mode = os.getenv("SBOX_MAPMODE")

		if (mapping_mode == nil) then
			mapping_mode = "simple"
		end

		local rule = nil
		local chain = nil

		sb.log("debug", "trying exec_policy_chains..")
		chain = find_chain(modes[mapping_mode].exec_policy_chains, binaryname)
		if (chain ~= nil) then
			sb.log("debug", "chain found, find rule for "..mapped_file)
			rule = find_rule(chain, func_name, mapped_file)
		end
		if (rule ~= nil) then
			sb.log("debug", "rule found..")
			exec_policy = rule.exec_policy
		end
		
		if (exec_policy == nil) then
			-- there is no default policy for this mode
			sb.log("warning",
				"sb_execve_postprocess: No exec_policy for "..filename)
			return 1, mapped_file, filename, #argv, argv, #envp, envp
		end
	end

	-- Exec policy found.

	if (exec_policy.log_level ~= nil) then
		sb.log(exec_policy.log_level, exec_policy.log_message)
	end

	if (exec_policy.deny_exec == true) then
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end

	if (exec_policy.name == nil) then
		sb.log("debug", "Applying nameless exec_policy")
	else
		sb.log("debug", string.format("Applying exec_policy '%s'",
			exec_policy.name))
	end

	-- End of generic part. Rest of postprocessing depends on type of
	-- the executable.

	if (exec_type == "native") then
		sb.log("debug", string.format("sb_execve_postprocess:type=%s", exec_type))
		return sb_execve_postprocess_native_executable(rule,
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	else
		-- all other exec_types: allow exec with orig.args
		sb.log("debug", string.format("sb_execve_postprocess:type=%s", exec_type))
		return 1, mapped_file, filename, #argv, argv, #envp, envp
	end
end

