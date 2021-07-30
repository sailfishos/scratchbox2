-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Portion Copyright (c) 2008 Nokia Corporation.
-- (exec postprocessing code written by Lauri T. Aarnio at Nokia)
--
-- Licensed under MIT license

-- Load session-specific exec-related settings
do_file(session_dir .. "/exec_config.lua")
do_file(session_dir .. "/cputransp_config.lua")

isprefix = sb.isprefix

all_exec_policies = nil

-- Load mode-specific rules.
-- A mode file must define two variables:
--  1. rule_file_interface_version (string) is checked and must match,
--     mismatch is a fatal error.
--  2. all_exec_policies (array)
-- Additionally, following variables may be modified:
-- "enable_cross_gcc_toolchain" (default=true): All special processing
--     for the gcc-related tools (gcc,as,ld,..) will be disabled if set
--     to false.
--
function load_and_check_exec_rules()

	-- initialize global variables:
	rule_file_interface_version = nil

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
	-- Version 102:
	-- - read "enable_cross_gcc_toolchain" from
	--   ruletree, it is defined originally
	--   in mode's "config.lua" file (which is
	--   never loaded directly)
	-- Version 203:
	-- - exec policy is selected by sb_exec.c always.
	local current_rule_interface_version = "203"

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
end

load_and_check_exec_rules()

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
	end

	set_ld_preload(envp, new_preload)
	return true
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

for k, v in pairs({conf_cputransparency_target, conf_cputransparency_native}) do
	if (v ~= nil and string.match(v.cmd, "qemu")) then
		v.method_is_qemu = true
	end
	if (v ~= nil and string.match(v.cmd, "sbrsh")) then
		v.method_is_sbrsh = true
	end
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

	local s_target_root = sbox_target_root
	if not string.match(s_target_root, "/$") then
		-- Add a trailing /
		s_target_root = s_target_root.."/"
	end

	local file_in_device = mapped_file;

	-- Check the file to execute; fail if the file can
	-- not be located on the device
	if isprefix(s_target_root, mapped_file) then
		local trlen = string.len(s_target_root)
		file_in_device = string.sub(file_in_device, trlen)
	elseif isprefix(sbox_user_home_dir, mapped_file) then
		-- no change
	else
		sb.log("error", string.format(
			"Binary must be under target (%s) or"..
			" home when using sbrsh", s_target_root))
		return -1, mapped_file, filename, #argv, argv, #envp, envp
	end

	-- Check directory
	local dir_in_device = sb.getcwd()

	if isprefix(s_target_root, dir_in_device) then
		local trlen = string.len(s_target_root)
		dir_in_device = string.sub(dir_in_device, trlen)
	elseif isprefix(sbox_user_home_dir, dir_in_device) then
		-- no change
	else
		sb.log("warning", string.format(
			"Executing binary with bogus working"..
			" directory (/tmp) because sbrsh can only"..
			" see %s and %s\n",
			s_target_root, sbox_user_home_dir))
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
    exec_type, mapped_file, filename, argv, envp, conf_cputransparency)

	if debug_messages_enabled then
		sb.log("debug", "postprocessing cpu_transparency for " .. filename)
	end

	if conf_cputransparency.method_is_qemu then
		local new_envp = {}
		local new_argv = {}
		local new_filename

		if conf_cputransparency.qemu_argv == nil then
			table.insert(new_argv, conf_cputransparency.cmd)
			new_filename = conf_cputransparency.cmd
		else
			for i = 1, table.maxn(conf_cputransparency.qemu_argv) do
				table.insert(new_argv, conf_cputransparency.qemu_argv[i])
			end
			new_filename = conf_cputransparency.qemu_argv[1]
		end

		if conf_cputransparency.qemu_env ~= nil then
			for i = 1, table.maxn(conf_cputransparency.qemu_env) do
				table.insert(new_envp, conf_cputransparency.qemu_env[i])
			end
		end

		-- target runtime linker comes from /
		table.insert(new_argv, "-L")
		table.insert(new_argv, "/")

		if conf_cputransparency.has_argv0_flag then
			-- set target argv[0]
			table.insert(new_argv, "-0")
			table.insert(new_argv, argv[1])
		end

		if conf_cputransparency.qemu_has_libattr_hack_flag then
			-- For ARM emulation:
			-- a nasty bug exists in some older libattr library
			-- version (e.g. it causes "ls -l" to crash), this
			-- flag enables a hack in Qemu which makes
			-- libattr to work correctly even if it uses incorrect
			-- system call format.
			table.insert(new_argv, "-libattr-hack")
		end

		if conf_cputransparency.qemu_has_env_control_flags then
			for i = 1, #envp do
				-- drop LD_TRACE_* from target environment
				if string.match(envp[i], "^LD_TRACE_.*") then
					-- .. and move to qemu command line 
					table.insert(new_argv, "-E")
					table.insert(new_argv, envp[i])
				elseif string.match(envp[i], "^__SB2_LD_PRELOAD=.*") then
					-- FIXME: This will now drop application's
					-- LD_PRELOAD. This is not really what should 
					-- be done... To Be Fixed.
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
		if conf_cputransparency.qemu_ld_library_path == "" then
			qemu_ldlibpath = "LD_LIBRARY_PATH=" .. host_ld_library_path
		else
			qemu_ldlibpath = conf_cputransparency.qemu_ld_library_path
		end
		if conf_cputransparency.qemu_ld_preload == "" then
			qemu_ldpreload = "LD_PRELOAD=" ..  host_ld_preload
		else
			qemu_ldpreload = conf_cputransparency.qemu_ld_preload
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
	elseif conf_cputransparency.method_is_sbrsh then
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

	assert(exec_policy_name ~= nil)

	sb.log("error", "sb_execve_postprocess called. "..exec_policy_name.." "..mapped_file.." "..filename);

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
		sb.log("error", "sb_execve_postprocess called to process native binary "..binaryname..", "..mapped_file)
		return sb_execve_postprocess_native_executable(
			exec_policy, exec_type, mapped_file,
			filename, argv, envp)
	elseif (exec_type == "cpu_transparency") then
		return sb_execve_postprocess_cpu_transparency_executable(
			exec_policy, exec_type, mapped_file,
			filename, argv, envp, conf_cputransparency_target)
	elseif (exec_type == "static") then
		if (conf_cputransparency_native ~= nil and conf_cputransparency_native.cmd ~= "") then
			return sb_execve_postprocess_cpu_transparency_executable(
				exec_policy, exec_type, mapped_file,
				filename, argv, envp, conf_cputransparency_native)
		end
		-- [see comment in sb_exec.c]
		set_ld_preload(envp, host_ld_preload)
		set_ld_library_path(envp, host_ld_library_path)
		return 0, mapped_file, filename, #argv, argv, #envp, envp
	end
	
	-- all other exec_types: allow exec with orig.args
	return 1, mapped_file, filename, #argv, argv, #envp, envp
end

exec_engine_loaded = true

