-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Portion Copyright (c) 2008 Nokia Corporation.
-- (exec postprocessing code written by Lauri T. Aarnio at Nokia)
--
-- Licensed under MIT license

-- Load session-specific exec-related settings
do_file(session_dir .. "/exec_config.lua")

--
-- argv&envp mangling rules are separated into two files
-- 	argvenvp_misc.lua - rules for misc binaries
-- 	argvenvp_gcc.lua  - rules for gcc
--
-- With these rules, script create_argvmods_rules.lua generates
-- the actual rules that are loaded into sb2, and writes those
-- to SBOX_SESSION_DIR/argvmods_misc.lua and
-- SBOX_SESSION_DIR/argvmods_gcc.lua. One of these is loaded here.
--
-- Syntax is of the form:
--
-- rule = {
--	path_prefixes = {"/list/of", "/possible/path/prefixes"},
-- 	add_head = {"list", "of", "args", "to", "prepend"},
-- 	add_options = {"list", "of", "options", "to", "add",
--		 "after", "argv[0]"},
-- 	add_tail = {"these", "are", "appended"},
-- 	remove = {"args", "to", "remove"},
-- 	new_filename = "exec-this-binary-instead",
-- 	disable_mapping = 1 -- set this to disable mappings
-- }
-- argvmods[name] = rule
--
-- Environment modifications are not supported yet, except for disabling
-- mappings.
--
-- TODO:
--
-- * new_filename should probably be replaced by integrating argv/envp
--   mangling with the path mapping machinery.

exec_policy_rules = nil

all_exec_policies = nil

-- Load mode-specific rules.
-- A mode file must define three variables:
--  1. rule_file_interface_version (string) is checked and must match,
--     mismatch is a fatal error.
--  2. all_exec_policies (array)
--  3. exec_policy_rules (array) contains default execution policies;
--     real path (mapped path) is used as the key. A default exec_policy
--     must be present.
-- Additionally, following variables may be modified:
-- "enable_cross_gcc_toolchain" (default=true): All special processing
--     for the gcc-related tools (gcc,as,ld,..) will be disabled if set
--     to false.
--
function load_and_check_exec_rules()

	-- initialize global variables:
	rule_file_interface_version = nil
	exec_policy_rules = nil

	tools = tools_root
	if (not tools) then
		tools = "/"
	end

	-- INTERFACE VERSION between this file, the
	-- exec mapping code (argvenp.lua) and the
	-- rule files:
	--
	-- Version 100:
	-- - fs rules and exec rules to separate files
	-- Version 101:
	-- - fs rules were updated, this was bumped
	--   to keep these in sync.
	local current_rule_interface_version = "101"

	do_file(exec_rule_file_path)

	-- fail and die if interface version is incorrect
	if (rule_file_interface_version == nil) or 
           (type(rule_file_interface_version) ~= "string") then
		sb.log("error", string.format(
			"Fatal: Exec rule file interface version check failed: "..
			"No version information in %s",
			rule_file_path))
		os.exit(99)
	end
	if rule_file_interface_version ~= current_rule_interface_version then
		sb.log("error", string.format(
			"Fatal: Exec rule file interface version check failed: "..
			"got %s, expected %s", rule_file_interface_version,
			current_rule_interface_version))
		os.exit(98)
	end

        if (type(exec_policy_rules) ~= "table") then
		sb.log("error", "'fs_mapping_rule' is not an array.");
		os.exit(97)
	end
end

argvmods = {}

load_and_check_exec_rules()

local argvmods_file_path

if (enable_cross_gcc_toolchain == true) then
	-- only map gcc & friends if a cross compiler has been defined,
	-- and it has not been disabled by the mapping rules:
	-- (it include the "misc" rules, too)
	argvmods_file_path = session_dir .. "/argvmods_gcc.lua"
else
	argvmods_file_path = session_dir .. "/argvmods_misc.lua"
end

-- load in autimatically generated argvmods file
if sb.path_exists(argvmods_file_path) then
	do_file(argvmods_file_path)
	if debug_messages_enabled then
		sb.log("debug", string.format(
		    "loaded argvmods from '%s'",
		    argvmods_file_path))
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
			"sbox_execve_preprocess(): %s", filename))
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
		if (not am.add_options) then am.add_options = {} end
		if (not am.add_tail) then am.add_tail = {} end

		if (debug_messages_enabled) then
			sb.log("debug", string.format(
				"argvmods[%s] found\n", filename))
		end

		-- head additions
		for i = 1, table.maxn(am.add_head) do
			table.insert(new_argv, am.add_head[i])
		end
	
		-- argv[0] (n.b. here first element is [1]
		table.insert(new_argv, argv[1])

		-- additional options
		for i = 1, table.maxn(am.add_options) do
			table.insert(new_argv, am.add_options[i])
		end

		-- populate new_argv, skip those that are to be removed
		for i = 2, table.maxn(argv) do
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

function get_users_ld_library_path(envp)
	local k
	for k = 1, table.maxn(envp) do
		if (string.match(envp[k], "^__SB2_LD_LIBRARY_PATH=")) then
			return string.gsub(envp[k], "^__SB2_LD_LIBRARY_PATH=", "", 1)
		end
	end
	return ""
end

function locate_ld_preload(envp)
	local k
	for k = 1, table.maxn(envp) do
		if (string.match(envp[k], "^LD_PRELOAD=")) then
			return k
		end
	end
	return -1
end

function get_users_ld_preload(envp)
	local k
	for k = 1, table.maxn(envp) do
		if (string.match(envp[k], "^__SB2_LD_PRELOAD=")) then
			return string.gsub(envp[k], "^__SB2_LD_PRELOAD=", "", 1)
		end
	end
	return ""
end

function join_paths(p1,p2)
	if (p1 == nil or p1 == "") then
		return p2
	end
	if (p2 == nil or p2 == "") then
		return p1
	end
	-- p1 and p2 are not empty
	if (string.match(p1, ":$")) then
		return p1..p2
	end
	return p1..":"..p2
end

function set_ld_library_path(envp, new_path)
	local ld_library_path_index = locate_ld_library_path(envp)
	if (ld_library_path_index > 0) then
		envp[ld_library_path_index] =
			"LD_LIBRARY_PATH=" .. new_path
		if debug_messages_enabled then
			sb.log("debug", "Replaced LD_LIBRARY_PATH")
		end
	else
		table.insert(envp, "LD_LIBRARY_PATH=" .. new_path)
		if debug_messages_enabled then
			sb.log("debug", "Added LD_LIBRARY_PATH")
		end
	end
end

-- Set LD_LIBRARY_PATH: modifies "envp"
function setenv_native_app_ld_library_path(exec_policy, envp)
	local new_path

	if (exec_policy.native_app_ld_library_path ~= nil) then
		-- attribute "native_app_ld_library_path" overrides everything else:
		new_path = exec_policy.native_app_ld_library_path
	elseif ((exec_policy.native_app_ld_library_path_prefix ~= nil) or
		(exec_policy.native_app_ld_library_path_suffix ~= nil)) then
		-- attributes "native_app_ld_library_path_prefix" and
		-- "native_app_ld_library_path_suffix" extend user's value:
		local libpath = get_users_ld_library_path(envp)
		new_path = join_paths(
			exec_policy.native_app_ld_library_path_prefix,
			join_paths(libpath,
				exec_policy.native_app_ld_library_path_suffix))
	else
		new_path = nil
	end

	-- Set the value:
	if (new_path == nil) then
		if debug_messages_enabled then
			sb.log("debug", "No value for LD_LIBRARY_PATH, using host's path")
		end
		-- Use host's original value
		new_path = host_ld_library_path
	end

	set_ld_library_path(envp, new_path)
	return true
end

function set_ld_preload(envp, new_preload)
	-- Set the value:
	local ld_preload_index = locate_ld_preload(envp)
	if (ld_preload_index > 0) then
		envp[ld_preload_index] = "LD_PRELOAD=" .. new_preload
		if debug_messages_enabled then
			sb.log("debug", "Replaced LD_PRELOAD="..new_preload)
		end
	else
		table.insert(envp, "LD_PRELOAD=" .. new_preload)
		if debug_messages_enabled then
			sb.log("debug", "Added LD_PRELOAD="..new_preload)
		end
	end
end

-- Set LD_PRELOAD: modifies "envp"
function setenv_native_app_ld_preload(exec_policy, envp)
	local new_preload

	if (exec_policy.native_app_ld_preload ~= nil) then
		new_preload = exec_policy.native_app_ld_preload
	elseif (exec_policy.native_app_ld_preload_prefix ~= nil or
	        exec_policy.native_app_ld_preload_suffix ~= nil) then
		local user_preload = get_users_ld_preload(envp)
		new_preload = join_paths(
			exec_policy.native_app_ld_preload_prefix,
			join_paths(user_preload,
				exec_policy.native_app_ld_preload_suffix))
	else
		new_preload = host_ld_preload
		-- See if fakeroot is needed. The exec policy
		-- didn't say anything about that; now there are
		-- two ways how fakeroot may get in: 
		if sb.get_session_perm() == "root" then
			-- session was entered with -R 
			new_preload = new_preload..":"..host_ld_preload_fakeroot
		else
			-- check if fakeroot session was created inside
			-- the sb2 session. User's LD_PRELOAD variable
			-- wiil reveal that.
			local users_ld_preload = get_users_ld_preload(envp)
			if (users_ld_preload ~= "") then
				if string.find(users_ld_preload,"libfakeroot") then
					-- need to use fakeroot.
					new_preload = new_preload..":"..host_ld_preload_fakeroot
				end
			end
		end
	end

	set_ld_preload(envp, new_preload)
	return true
end

--
-- Tries to find exec_policy for given binary using exec_policy_rules.
--
-- The exec policy selection table can contain three types of rules:
--      { prefix = "/path/prefix", exec_policy_name = "policyname" }
--      { path = "/exact/path/to/program", exec_policy_name = "policyname" }
--      { dir = "/directory/path", exec_policy_name = "policyname" }
-- Other types of rules are not anymore supported.
--
-- Returns: 1, exec_policy when exec_policy was found, otherwise
-- returns 0, nil.
--
function sb_find_exec_policy(mapped_file)
	if debug_messages_enabled then
		sb.log("debug", "sb_find_exec_policy for "..mapped_file)
	end
	for i = 1, table.maxn(exec_policy_rules) do
		local rule = exec_policy_rules[i]
		min_path_len = sb.test_path_match(mapped_file,
			rule.dir, rule.prefix, rule.path)
		if min_path_len >= 0 then
			if debug_messages_enabled then
				sb.log("debug", "exec policy found: "..rule.exec_policy_name)
			end
			return 1, rule.exec_policy_name
		end
	end
	return 0, nil
end

-- ------------------------------------
-- returns args_ok, rule, exec_policy_name
function check_exec_policy(exec_policy_name, filename, mapped_file)

	-- if exec_policy_name was not provided by the caller (i.e. not
	-- provided by the mapping rule), look up the policy from
	-- exec_policy_rules array.
	if (exec_policy_name == nil) then
		local res

		if debug_messages_enabled then
			sb.log("debug", "trying exec_policy_rules..")
		end
		res, exec_policy_name = sb_find_exec_policy(mapped_file)
		if (res == 0) or (exec_policy_name == nil) then
			-- there is no default policy for this mode
			sb.log("notice",
				"sb_execve_postprocess: No exec_policy for "..filename)
			return false, exec_policy_name
		end
	end

	-- Exec policy is OK.
	
	return true, exec_policy_name
end

-- ------------------------------------
-- Script interpreter mapping.

-- This is called from C:
-- returns: policy, result, mapped_interpreter, #argv, argv, #envp, envp
-- "result" is one of:
--  0: argv / envp were modified; mapped_interpreter was set
--  1: argv / envp were not modified; mapped_interpreter was set
--  2: argv / envp were not modified; caller should call ordinary path 
--	mapping to find the interpreter
-- -1: deny exec.
function sb_execve_map_script_interpreter(exec_policy_name, interpreter,
	interp_arg, mapped_script_filename, orig_script_filename, argv, envp)

	local args_ok
	args_ok, exec_policy_name = check_exec_policy(
		exec_policy_name, orig_script_filename, mapped_script_filename) 

	if args_ok == false then
		-- no exec policy. Deny exec, we can't find the interpreter
		sb.log("error", "Unable to map script interpreter.");
		return exec_policy_name, -1, interpreter, #argv, argv, #envp, envp
	end

	-- Exec policy found.
	local exec_policy = get_exec_policy_by_name(exec_policy_name)

	-- exec policy is OK.

	if (exec_policy.script_log_level ~= nil) then
		sb.log(exec_policy.script_log_level,
			exec_policy.script_log_message)
	end

	if (exec_policy.script_deny_exec == true) then
		return exec_policy_name, -1, interpreter, #argv, argv, #envp, envp
	end

	if debug_messages_enabled then
		if (exec_policy.name == nil) then
			sb.log("debug", "Applying nameless exec_policy to script")
		else
			sb.log("debug", string.format(
				"Applying exec_policy '%s' to script",
				exec_policy.name))
		end
	end

	if (exec_policy.script_interpreter_rules ~= nil) then
		local min_path_len = 0
		local rule = nil
		local exec_pol_2, mapped_interpreter, ro_flag

		-- FIXME: 4th parameter of find_rule() should be binary_name
		rule, min_path_len = find_rule(exec_policy.script_interpreter_rules,
			"map_script_interpreter", interpreter, nil)

		if (rule) then
			exec_pol_2, mapped_interpreter, ro_flag = sbox_execute_rule(
				interpreter, "map_script_interpreter",
				interpreter, interpreter, rule, rule)
		
			if exec_policy.script_set_argv0_to_mapped_interpreter then
				argv[1] = mapped_interpreter
				return exec_pol_2, 0, 
					mapped_interpreter, #argv, argv, #envp, envp
			else
				return exec_pol_2, 1, 
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
	return exec_policy_name, 2, interpreter, #argv, argv, #envp, envp
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

function sb_execve_postprocess_native_executable(exec_policy,
	exec_type, mapped_file, filename, argv, envp)

	-- Native binary. See what we need to do with it...
	if debug_messages_enabled then
		sb.log("debug", "sb_execve_postprocess_native_executable")
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

		-- Ignore RPATH and RUNPATH information:
		-- This will prevent accidental use of host's libraries,
		-- if the binary has been set to use RPATHs. 
		-- (it would be nice if we could log warnings about them,
		-- but currently there is no easy way to do that)
		if (exec_policy.native_app_ld_so_supports_rpath_prefix) then
			table.insert(new_argv, "--rpath-prefix")
			table.insert(new_argv, exec_policy.native_app_ld_so_rpath_prefix)
		else
			table.insert(new_argv, "--inhibit-rpath")
			table.insert(new_argv, "") -- empty "LIST" == the binary itself
		end

		if (exec_policy.native_app_ld_so_supports_nodefaultdirs) then
			table.insert(new_argv, "--nodefaultdirs")
		end

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
	end

	-- Ensure that the binary has a LD_LIBRARY_PATH,
	-- either a non-standard one from the policy,
	-- or the original host's LD_LIBRARY_PATH. It
	-- won't work without any.
	if setenv_native_app_ld_library_path(exec_policy, new_envp) then
		updated_args = 1
	end
	-- Also, set that LD_PRELOAD
	if setenv_native_app_ld_preload(exec_policy, new_envp) then
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
		if debug_messages_enabled then
			sb.log("debug", string.format("setting LOCPATH=%s",
			    exec_policy.native_app_locale_path))
		end
		table.insert(new_envp, "LOCPATH=" ..
		    exec_policy.native_app_locale_path)
		table.insert(new_envp, "NLSPATH=" ..
		    exec_policy.native_app_locale_path)
		updated_args = 1
	end

	if exec_policy.native_app_gconv_path ~= nil then
		if debug_messages_enabled then
			sb.log("debug", string.format("setting GCONV_PATH=%s",
			    exec_policy.native_app_gconv_path))
		end
		table.insert(new_envp, "GCONV_PATH=" ..
		    exec_policy.native_app_gconv_path)
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

if string.match(sbox_cputransparency_cmd, "qemu") then
	cputransparency_method_is_qemu = true
end
if string.match(sbox_cputransparency_cmd, "sbrsh") then
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

function sb_execve_postprocess_sbrsh(exec_policy,
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
		if debug_messages_enabled then
			sb.log("debug", "LD_PRELOAD not found")
		end
	else
		if debug_messages_enabled then
			sb.log("debug", string.format("LD_PRELOAD was %s",
				ld_preload_tbl[1]))
		end
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
			if debug_messages_enabled then
				sb.log("debug", "set LD_PRELOAD to "..new_ld_preload)
			end
		else
			if debug_messages_enabled then
				sb.log("debug", "nothing left, run without LD_PRELOAD")
			end
		end
	end

	-- environment&args were changed
	return 0, new_filename, filename, #new_argv, new_argv,
		#new_envp, new_envp
end

function sb_execve_postprocess_cpu_transparency_executable(exec_policy,
    exec_type, mapped_file, filename, argv, envp)

	if debug_messages_enabled then
		sb.log("debug", "postprocessing cpu_transparency for " .. filename)
	end

	if cputransparency_method_is_qemu then
		local new_envp = {}
		local new_argv = {}
		local new_filename

		if conf_cputransparency_qemu_argv == nil then
			table.insert(new_argv, sbox_cputransparency_cmd)
			new_filename = sbox_cputransparency_cmd
		else
			for i = 1, table.maxn(conf_cputransparency_qemu_argv) do
				table.insert(new_argv, conf_cputransparency_qemu_argv[i])
			end
			new_filename = conf_cputransparency_qemu_argv[1]
		end

		if conf_cputransparency_qemu_env ~= nil then
			for i = 1, table.maxn(conf_cputransparency_qemu_env) do
				table.insert(new_envp, conf_cputransparency_qemu_env[i])
			end
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

		local needs_libfakeroot = false
		if sb.get_session_perm() == "root" then
			needs_libfakeroot = true
		end

		if conf_cputransparency_qemu_has_env_control_flags then
			for i = 1, #envp do
				-- drop LD_TRACE_* from target environment
				if string.match(envp[i], "^LD_TRACE_.*") then
					-- .. and move to qemu command line 
					table.insert(new_argv, "-E")
					table.insert(new_argv, envp[i])
				elseif string.match(envp[i], "^__SB2_LD_PRELOAD=.*") then
					if string.match(envp[i], "libfakeroot") then
						if needs_libfakeroot == false then
							table.insert(new_envp, "SBOX_SESSION_PERM=root")
						end
						needs_libfakeroot = true
					end
					-- FIXME: This will now drop application's
					-- LD_PRELOAD. This is not really what should 
					-- be done; instead we should only drop 
					-- libfakeroot, not everything... To Be Fixed.
				else
					table.insert(new_envp, envp[i])
				end
			end
		else
			-- copy environment. Some things will be broken with
			-- this qemu (for example, prelinking won't work, etc)
			new_envp = envp
		end

		hack_envp = { }
		for i = 1, #new_envp do
			if string.match(new_envp[i], "^GCONV_PATH=.*") or
			   string.match(new_envp[i], "^NLSPATH=.*") or
			   string.match(new_envp[i], "^LOCPATH=.*") then
				-- skip
			else
				table.insert(hack_envp, new_envp[i])
			end
		end
		new_envp, hack_envp = hack_envp, nil

		-- libsb2 will replace LD_PRELOAD and LD_LIBRARY_PATH
		-- env.vars, we don't need to worry about what the
		-- application will see in those - BUT we need
		-- to set those variables for Qemu itself.
		-- Fortunately that is easy: 
		local qemu_ldlibpath
		local qemu_ldpreload
		if conf_cputransparency_qemu_ld_library_path == "" then
			qemu_ldlibpath = "LD_LIBRARY_PATH=" .. host_ld_library_path
		else
			qemu_ldlibpath = conf_cputransparency_qemu_ld_library_path
		end
		if conf_cputransparency_qemu_ld_preload == "" then
			qemu_ldpreload = "LD_PRELOAD=" ..  host_ld_preload
		else
			qemu_ldpreload = conf_cputransparency_qemu_ld_preload
		end

		-- NOTE/FIXME: This still assumes that the name of the libfakeroot
		-- library is the same as what is used on the host. This is 
		-- usually (always?) the case, and if it isn't, there is
		-- something fatally wrong anyway. That is why 
		-- "host_ld_preload_fakeroot" is used here, even if "host_*"
		-- should not be mixed to things that might actually come from
		-- the target. But using libsb2 and libfakeroot at the same
		-- time is a complex thing...
		if needs_libfakeroot then
			qemu_ldpreload = qemu_ldpreload ..  ":" .. host_ld_preload_fakeroot
		end

		table.insert(new_envp, qemu_ldlibpath)
		table.insert(new_envp, qemu_ldpreload)

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
		return sb_execve_postprocess_sbrsh(exec_policy,
    			exec_type, mapped_file, filename, argv, envp)
	end

	-- no changes
	return 1, mapped_file, filename, #argv, argv, #envp, envp
end

function get_exec_policy_by_name(ep_name)
	if (all_exec_policies ~= nil) then
		for i = 1, table.maxn(all_exec_policies) do
			if all_exec_policies[i].name == ep_name then
				if (debug_messages_enabled) then
					sb.log("debug", "Found Exec policy "..ep_name)
				end
				return all_exec_policies[i]
			end
		end
	end
	if (debug_messages_enabled) then
		sb.log("debug", "FAILED to find Exec policy "..ep_name)
	end
	return nil
end


-- This is called from C:
function sb_execve_postprocess(exec_policy_name, exec_type,
	mapped_file, filename, binaryname, argv, envp)

	local args_ok
	args_ok, exec_policy_name = check_exec_policy(
		exec_policy_name, filename, mapped_file) 

	if args_ok == false then
		-- postprocessing is not needed / can't be done, but
		-- exec must be allowed.
		return 1, mapped_file, filename, #argv, argv, #envp, envp
	end

	-- Exec policy found.
	local exec_policy = get_exec_policy_by_name(exec_policy_name)
	if exec_policy == nil then
		sb.log("error", "Exec policy '"..exec_policy_name.."' not found.")
		-- Allow direct exec.
		return 1, mapped_file, filename, #argv, argv, #envp, envp
	end

	if (exec_policy.log_level ~= nil) then
		sb.log(exec_policy.log_level, exec_policy.log_message)
	end

	if (exec_policy.deny_exec == true) then
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end

	if debug_messages_enabled then
		if (exec_policy.name == nil) then
			sb.log("debug", "Applying nameless exec_policy")
		else
			sb.log("debug", string.format("Applying exec_policy '%s'",
				exec_policy.name))
		end
		sb.log("debug", string.format("sb_execve_postprocess:type=%s",
			exec_type))
	end

	if (exec_policy.name) then
		table.insert(envp, "__SB2_EXEC_POLICY_NAME="..exec_policy.name)
	end

	-- End of generic part. Rest of postprocessing depends on type of
	-- the executable.

	if (exec_type == "native") then
		return sb_execve_postprocess_native_executable(
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	elseif (exec_type == "cpu_transparency") then
		return sb_execve_postprocess_cpu_transparency_executable(
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	elseif (exec_type == "static") then
		-- [see comment in sb_exec.c]
		local ldlibpath
		local ldpreload
		ldpreload, ldlibpath = sbox_get_host_policy_ld_params()
		set_ld_preload(envp, ldpreload)
		set_ld_library_path(envp, ldlibpath)
		return 0, mapped_file, filename, #argv, argv, #envp, envp
	end
	
	-- all other exec_types: allow exec with orig.args
	return 1, mapped_file, filename, #argv, argv, #envp, envp
end

-- This is called from C:
function sbox_get_host_policy_ld_params()
	-- FIXME:
	-- in the future we should get these values from a "Host" exec policy,
	-- but can't do so before the exec policy conventions dictate
	-- that a "Host" policy must exist. Now the values are hardcoded:
	local ldpreload
	if sb.get_session_perm() == "root" then
		ldpreload = host_ld_preload ..  ":" .. host_ld_preload_fakeroot
	else
		ldpreload = host_ld_preload
	end
	return ldpreload, host_ld_library_path
end

exec_engine_loaded = true

