-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Portion Copyright (c) 2008 Nokia Corporation.
-- (exec postprocessing code written by Lauri T. Aarnio at Nokia)
--
-- Licensed under MIT license

--
-- argv&envp mangling rules are separated into two files
-- 	argvenvp_misc.lua - rules for misc binaries
-- 	argvenvp_gcc.lua  - rules for gcc
--
-- With these rules, script create_argvmods_rules.lua generates
-- the actual rules that are loaded into sb2.  Generated files
-- are placed under SBOX_SESSION_DIR/argvmods and they are named
-- like argvmods_xxx.lua.
--
-- Syntax is of the form:
--
-- rule = {
-- 	name = "binary-name",
--	path_prefixes = {"/list/of", "/possible/path/prefixes"},
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

argvmods = {}

-- only map gcc & friends if a cross compiler has been defined,
-- and it has not been disabled by the mapping rules:
if (enable_cross_gcc_toolchain == true) then
	local gcc_argvmods_file_path =
	    session_dir .. "/argvmods/argvmods_gcc.lua"
	
	if sb.path_exists(gcc_argvmods_file_path) then
		-- load in autimatically generated argvmods for gcc
		do_file(gcc_argvmods_file_path)
		if debug_messages_enabled then
			sb.log("debug", string.format(
			    "loaded argvmods for gcc from '%s'",
			    gcc_argvmods_file_path))
		end
	end
end

--
-- Always load argvmods for misc binaries.
--
local misc_argvmods_file_path = session_dir .. "/argvmods/argvmods_misc.lua"
if sb.path_exists(misc_argvmods_file_path) then
	do_file(misc_argvmods_file_path)
	if debug_messages_enabled then
		sb.log("debug", string.format(
		    "loaded argvmods for misc binaries from '%s'",
		    misc_argvmods_file_path))
	end
end

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

	if (debug_messages_enabled) then
		sb.log("debug", string.format(
			"sbox_execve_preprocess(): %s\n", filename))
	end
	
	new_envp = envp

	local am = argvmods[binaryname]
	if (am ~= nil) then
		local prefix_match_found = false
		for i = 1, table.maxn(am.path_prefixes) do
			if isprefix(am.path_prefixes[i], filename) then
				prefix_match_found = true
				break
			end
		end
		if (not prefix_match_found) then
			am = nil
		end
	end

	if (am ~= nil) then
		if (not am.remove) then am.remove = {} end
		if (not am.add_head) then am.add_head = {} end
		if (not am.add_tail) then am.add_tail = {} end

		if (debug_messages_enabled) then
			sb.log("debug", string.format(
				"argvmods[%s] found\n", filename))
		end

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

function locate_ld_library_path(envp)
	local k
	for k = 1, table.maxn(envp) do
		if (string.match(envp[k], "^LD_LIBRARY_PATH=")) then
			return k
		end
	end
	return -1
end

-- Return path to be used as LD_LIBRARY_PATH for native applications
--
function get_native_app_ld_library_path(exec_policy, envp)

	-- attribute "native_app_ld_library_path" overrides everything else:
	if (exec_policy.native_app_ld_library_path ~= nil) then
		return exec_policy.native_app_ld_library_path
	end

	-- attributes "native_app_ld_library_path_prefix" and
	-- "native_app_ld_library_path_suffix" extend the old value:
	if ((exec_policy.native_app_ld_library_path_prefix ~= nil) or
	    (exec_policy.native_app_ld_library_path_suffix ~= nil)) then
		local ld_library_path_index = locate_ld_library_path(envp)
		local libpath = nil
		if ld_library_path_index > 0 then
			libpath = string.gsub(envp[ld_library_path_index],
				"^LD_LIBRARY_PATH=", "", 1)
		end
		if (exec_policy.native_app_ld_library_path_prefix ~= nil) then
			if libpath ~= nil then
				libpath = exec_policy.native_app_ld_library_path_prefix ..
					":" .. libpath
			else
				libpath = exec_policy.native_app_ld_library_path_prefix
			end
		end
		if (exec_policy.native_app_ld_library_path_suffix ~= nil) then
			if libpath ~= nil then
				libpath = libpath .. ":" ..
					 exec_policy.native_app_ld_library_path_suffix
			else
				libpath = exec_policy.native_app_ld_library_path_suffix
			end
		end
		return libpath
	end

	return nil
end

-- Set LD_LIBRARY_PATH: modifies "envp"
--
function setenv_native_app_ld_library_path(exec_policy, envp)

	local ld_library_path_index = locate_ld_library_path(envp)
	local new_path = get_native_app_ld_library_path(exec_policy, envp)

	-- Set the value:
	if (new_path ~= nil) then
		if (ld_library_path_index > 0) then
			envp[ld_library_path_index] =
				"LD_LIBRARY_PATH=" .. new_path
			sb.log("debug", "Replaced LD_LIBRARY_PATH")
		else
			table.insert(envp, "LD_LIBRARY_PATH=" .. new_path)
			sb.log("debug", "Added LD_LIBRARY_PATH")
		end
	else
		sb.log("debug", "No value for LD_LIBRARY_PATH")
	end
end

-- ------------------------------------
-- returns args_ok, rule, exec_policy 
function check_rule_and_policy(rule, exec_policy, filename, mapped_file)

	-- First, if either rule or the exec policy is a string, something
	-- has failed during the previous steps or mapping has been disabled;
	-- => fail
	if ((type(rule) == "string") or (type(exec_policy) == "string")) then
		local rs = ""
		local eps = ""
		if (type(rule) == "string") then
			rs = rule
		end
		if (type(exec_policy) == "string") then
			eps = exec_policy
		end

		sb.log("debug", "check_rule_and_policy:Fail: "..rs..";"..eps);
		return false, rule, exec_policy
	end

	-- if exec_policy was not provided by the caller (i.e. not
	-- provided by the mapping rule), look up the policy from
	-- exec_policy_chains array.
	if (exec_policy == nil) then
		local res

		sb.log("debug", "trying exec_policy_chains..")
		res, exec_policy = sb_find_exec_policy(binaryname, mapped_file)
		if res == 0 then
			-- there is no default policy for this mode
			sb.log("notice",
				"sb_execve_postprocess: No exec_policy for "..filename)
			return false, rule, exec_policy
		end
	end

	-- Exec policy is OK.
	
	return true, rule, exec_policy
end

-- ------------------------------------
-- Script interpreter mapping.

-- This is called from C:
-- returns: rule, policy, result, mapped_interpreter, #argv, argv, #envp, envp
-- "result" is one of:
--  0: argv / envp were modified; mapped_interpreter was set
--  1: argv / envp were not modified; mapped_interpreter was set
--  2: argv / envp were not modified; caller should call ordinary path 
--	mapping to find the interpreter
-- -1: deny exec.
function sb_execve_map_script_interpreter(rule, exec_policy, interpreter,
	interp_arg, mapped_script_filename, orig_script_filename, argv, envp)

	local args_ok
	args_ok, rule, exec_policy = check_rule_and_policy(rule,
		exec_policy, orig_script_filename, mapped_script_filename) 

	if args_ok == false then
		-- no exec policy. Deny exec, we can't find the interpreter
		sb.log("error", "Unable to map script interpreter.");
		return rule, exec_policy, -1, interpreter, #argv, argv, #envp, envp
	end

	-- exec policy is OK.

	if (exec_policy.script_log_level ~= nil) then
		sb.log(exec_policy.script_log_level,
			exec_policy.script_log_message)
	end

	if (exec_policy.script_deny_exec == true) then
		return rule, exec_policy, -1, interpreter, #argv, argv, #envp, envp
	end

	if (exec_policy.name == nil) then
		sb.log("debug", "Applying nameless exec_policy to script")
	else
		sb.log("debug", string.format(
			"Applying exec_policy '%s' to script",
			exec_policy.name))
	end

	if (exec_policy.script_interpreter_rules ~= nil) then
		local min_path_len = 0
		local rule = nil
		local exec_pol_2, mapped_interpreter, ro_flag

		rule, min_path_len = find_rule(exec_policy.script_interpreter_rules,
			"map_script_interpreter", interpreter)

		if (rule) then
			exec_pol_2, mapped_interpreter, ro_flag = sbox_execute_rule(
				interpreter, "map_script_interpreter",
				interpreter, interpreter, rule)
		
			if exec_policy.script_set_argv0_to_mapped_interpreter then
				argv[1] = mapped_interpreter
				return rule, exec_pol_2, 0, 
					mapped_interpreter, #argv, argv, #envp, envp
			else
				return rule, exec_pol_2, 1, 
					mapped_interpreter, #argv, argv, #envp, envp
			end
		else
			sb.log("warning", string.format(
				"Failed to find script interpreter mapping rule for %s",
				interpreter))
		end
	end

	-- The default case:
	-- exec policy says nothing about the script interpreters.
	-- use ordinary path mapping to find it
	return rule, exec_policy, 2, interpreter, #argv, argv, #envp, envp
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

		local ld_lib_path = get_native_app_ld_library_path(
			exec_policy, new_envp)
		if (ld_lib_path ~= nil) then
			table.insert(new_argv, "--library-path")
			table.insert(new_argv, ld_lib_path)
		end

		-- Ignore RPATH and RUNPATH information:
		-- This will prevent accidental use of host's libraries,
		-- if the binary has been set to use RPATHs. 
		-- (it would be nice if we could log warnings about them,
		-- but currently there is no easy way to do that)
		table.insert(new_argv, "--inhibit-rpath")
		table.insert(new_argv, "") -- empty "LIST" == the binary itself

		-- NOTE/WARNING: The default ld.so (ld-linux.so) will loose
		-- argv[0], when the binary is executed by ld.so's
		-- command line (which we will be doing). It will always copy 
		-- the filename to argv[0].
		--
		-- We now have a patch for ld.so which introduces a new
		-- option, "--argv0 argument", and a flag is used to tell
		-- if a patched ld.so is available (the "sb2" script finds 
		-- that out during startup phase).
		--
		if (exec_policy.native_app_ld_so_supports_argv0) then
			table.insert(new_argv, "--argv0")
			-- C's argv[0] is in argv[1] here!
			table.insert(new_argv, argv[1])
			table.insert(new_argv, mapped_file)
		else
			-- Replace argv[0] by pathname:
			table.insert(new_argv, mapped_file)
		end
		first_argv_element_to_copy = 2

		updated_args = 1
	elseif ((exec_policy.native_app_ld_library_path ~= nil) or
	        (exec_policy.native_app_ld_library_path_prefix ~= nil) or
	        (exec_policy.native_app_ld_library_path_suffix ~= nil)) then
		-- Start the binary with a nonstandard LD_LIBRARY_PATH
		setenv_native_app_ld_library_path(exec_policy, new_envp)
		updated_args = 1
	end

	--
	-- When exec_policy contains field 'native_app_locale_path' we
	-- need to set environment variables $LOCPATH (and $NLSPATH) to
	-- point there.  Localization functions (e.g isalpha(), etc.)
	-- gets their locale specific information from $LOCPATH when
	-- it is set.
	--
	if exec_policy.native_app_locale_path ~= nil then
		sb.log("debug", string.format("setting LOCPATH=%s",
		    exec_policy.native_app_locale_path))
		table.insert(new_envp, "LOCPATH=" ..
		    exec_policy.native_app_locale_path)
		table.insert(new_envp, "NLSPATH=" ..
		    exec_policy.native_app_locale_path)
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

if string.match(sbox_cputransparency_method, "qemu") then
	cputransparency_method_is_qemu = true
end
if string.match(sbox_cputransparency_method, "sbrsh") then
	cputransparency_method_is_sbrsh = true
end

function split_to_tokens(text,delim)
	local results = {}
	local c
	for c in string.gmatch(text, delim) do
		table.insert(results, c)
	end
	return results
end

-- Remove selected elements from a table of strings.
-- Returns a new table containing the selected elements, or nil if none 
-- was found. Original table may be modified!
function pick_and_remove_elems_from_string_table(tbl,pattern)
	local res = nil

	if tbl ~= nil then
		local i = #tbl
		while (i > 0) do
			if string.match(tbl[i], pattern) then
				local elem = table.remove(tbl, i)
				if res == nil then
					res = {}
				end
				table.insert(res, elem)
			end
			i = i - 1
		end
	end
	return(res)
end

function sb_execve_postprocess_sbrsh(rule, exec_policy,
	exec_type, mapped_file, filename, argv, envp)

	local new_argv = split_to_tokens(sbox_cputransparency_method,"[^%s]+")

	if #new_argv < 1 then
		sb.log("error", "Invalid sbox_cputransparency_method set");
		-- deny
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end
	if (sbox_target_root == nil) or (sbox_target_root == "") then
		sb.log("error", 
			"sbox_target_root not set, "..
			"unable to execute the target binary");
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end

	sb.log("info", string.format("Exec:sbrsh (%s,%s,%s)",
		new_argv[1], sbox_target_root, mapped_file));

	local target_root = sbox_target_root
	if not string.match(target_root, "/$") then
		-- Add a trailing /
		target_root = target_root.."/"
	end

	local file_in_device = mapped_file;

	-- Check the file to execute; fail if the file can
	-- not be located on the device
	if isprefix(target_root, mapped_file) then
		local trlen = string.len(target_root)
		file_in_device = string.sub(file_in_device, trlen)
	elseif isprefix(sbox_user_home_dir, mapped_file) then
		-- no change
	else
		sb.log("error", string.format(
			"Binary must be under target (%s) or"..
			" home when using sbrsh", target_root))
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end

	-- Check directory
	local dir_in_device = sb.getcwd()

	if isprefix(target_root, dir_in_device) then
		local trlen = string.len(target_root)
		dir_in_device = string.sub(dir_in_device, trlen)
	elseif isprefix(sbox_user_home_dir, dir_in_device) then
		-- no change
	else
		sb.log("warning", string.format(
			"Executing binary with bogus working"..
			" directory (/tmp) because sbrsh can only"..
			" see %s and %s\n",
			target_root, sbox_user_home_dir))
		dir_in_device = "/tmp"
	end

	local new_envp = envp
	local new_filename = new_argv[1] -- first component of method
	
	if (sbox_sbrsh_config ~= nil) and (sbox_sbrsh_config ~= "") then
		table.insert(new_argv, "--config")
		table.insert(new_argv, sbox_sbrsh_config)
	end
	table.insert(new_argv, "--directory")
	table.insert(new_argv, dir_in_device)
	table.insert(new_argv, file_in_device)

	-- Append arguments for target process (skip argv[0],
	-- there isn't currently any way to give that over sbrsh)
	for i = 2, #argv do
		table.insert(new_argv, argv[i])
	end

	-- remove libsb2 from LD_PRELOAD
	local ld_preload_tbl = pick_and_remove_elems_from_string_table(
		new_envp, "^LD_PRELOAD=")
	if ld_preload_tbl == nil then
		sb.log("debug", "LD_PRELOAD not found")
	else
		sb.log("debug", string.format("LD_PRELOAD was %s",
			ld_preload_tbl[1]))
		local ld_preload_path = string.gsub(ld_preload_tbl[1],
			"^LD_PRELOAD=", "", 1)
		local ld_preload_components = split_to_tokens(ld_preload_path,
			"[^:]+")
		-- pick & throw away libsb2.so
		pick_and_remove_elems_from_string_table(ld_preload_components,
			sbox_libsb2)
		if #ld_preload_components > 0 then
			local new_ld_preload = table.concat(
				ld_preload_components, ":")
			table.insert(new_envp, "LD_PRELOAD="..new_ld_preload)
			sb.log("debug", "set LD_PRELOAD to "..new_ld_preload)
		else
			sb.log("debug", "nothing left, run without LD_PRELOAD")
		end
	end

	-- environment&args were changed
	return 0, new_filename, filename, #new_argv, new_argv,
		#new_envp, new_envp
end

function sb_execve_postprocess_cpu_transparency_executable(rule, exec_policy,
    exec_type, mapped_file, filename, argv, envp)

	sb.log("debug", "postprocessing cpu_transparency for " .. filename)

	if cputransparency_method_is_qemu then
		local new_envp = {}
		local new_argv = {}
		local new_filename = sbox_cputransparency_method

		table.insert(new_argv, sbox_cputransparency_method)
		-- drop LD_PRELOAD env.var.
		if conf_cputransparency_qemu_has_env_control_flags then
			table.insert(new_argv, "-U")
			table.insert(new_argv, "LD_PRELOAD")
		else
			table.insert(new_argv, "-drop-ld-preload")
		end
		-- target runtime linker comes from /
		table.insert(new_argv, "-L")
		table.insert(new_argv, "/")

		if conf_cputransparency_has_argv0_flag then
			-- set target argv[0]
			table.insert(new_argv, "-0")
			table.insert(new_argv, argv[1])
		end

		if conf_cputransparency_qemu_has_libattr_hack_flag then
			-- For ARM emulation:
			-- a nasty bug exists in some older libattr library
			-- version (e.g. it causes "ls -l" to crash), this
			-- flag enables a hack in Qemu which makes
			-- libattr to work correctly even if it uses incorrect
			-- system call format.
			table.insert(new_argv, "-libattr-hack")
		end

		if conf_cputransparency_qemu_has_env_control_flags then
			for i = 1, #envp do
				-- drop LD_TRACE_ from target environment
				if not string.match(envp[i], "^LD_TRACE_.*") then
					table.insert(new_envp, envp[i])
				else
					-- .. and move it to qemu command line 
					table.insert(new_argv, "-E")
					table.insert(new_argv, envp[i])
				end
			end
		else
			-- copy environment. Some things will be broken with
			-- this qemu (for example, prelinking won't work)
			new_envp = envp
		end

		-- unmapped file is exec'd
		table.insert(new_argv, filename)
		--
		-- Append arguments for target process (skip argv[0]
		-- as this is done using -0 switch).
		--
		for i = 2, #argv do
			table.insert(new_argv, argv[i])
		end

		-- environment&args were changed
		return 0, new_filename, filename, #new_argv, new_argv,
			#new_envp, new_envp
	elseif cputransparency_method_is_sbrsh then
		return sb_execve_postprocess_sbrsh(rule, exec_policy,
    			exec_type, mapped_file, filename, argv, envp)
	end

	-- no changes
	return 1, mapped_file, filename, #argv, argv, #envp, envp
end


-- This is called from C:
function sb_execve_postprocess(rule, exec_policy, exec_type,
	mapped_file, filename, binaryname, argv, envp)

	local args_ok
	args_ok, rule, exec_policy = check_rule_and_policy(rule,
		exec_policy, filename, mapped_file) 

	if args_ok == false then
		-- postprocessing is not needed / can't be done, but
		-- exec must be allowed.
		return 1, mapped_file, filename, #argv, argv, #envp, envp
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

	sb.log("debug", string.format("sb_execve_postprocess:type=%s",
		exec_type))

	if (exec_policy.name) then
		table.insert(envp, "__SB2_EXEC_POLICY_NAME="..exec_policy.name)
	end

	-- End of generic part. Rest of postprocessing depends on type of
	-- the executable.

	if (exec_type == "native") then
		return sb_execve_postprocess_native_executable(rule,
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	elseif (exec_type == "cpu_transparency") then
		return sb_execve_postprocess_cpu_transparency_executable(rule,
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	else
		-- all other exec_types: allow exec with orig.args
		return 1, mapped_file, filename, #argv, argv, #envp, envp
	end
end

