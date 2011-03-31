-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "100"
----------------------------------

-- If the permission token exists and contains "root", use fakeroot.
local fakeroot_ld_preload = ""
if sb.get_session_perm() == "root" then
	fakeroot_ld_preload = ":"..host_ld_preload_fakeroot
end

-- Exec policy rules.

default_exec_policy = {
	name = "Default",
	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}

-- For target binaries:
-- First, note that "foreign" binaries are easy to handle, no problem there.
-- But if CPU transparency method has not been set, then host CPU == target CPU:
-- we have "target's native" and "host's native" binaries, that would look 
-- identical (and valid!) to the kernel. But they need to use different 
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

emulate_mode_target_ld_so = nil		-- default = not needed
emulate_mode_target_ld_library_path_prefix = ""
emulate_mode_target_ld_library_path_suffix = nil

-- used if libsb2.so is not available in target_root:
emulate_mode_target_ld_library_path_suffix = nil

if (conf_target_sb2_installed) then
	--
	-- When libsb2 is installed to target we don't want to map
	-- the path where it is found.  For example gdb needs access
	-- to the library and dynamic linker.  So here we insert special
	-- rules on top of mapall_chain that prevents sb2 to map these
	-- paths.
	--
	if (conf_target_libsb2_dir ~= nil) then
		table.insert(mapall_chain.rules, 1,
		    { prefix = conf_target_libsb2_dir, use_orig_path = true,
		      readonly = true }) 
	end
	if (conf_target_ld_so ~= nil) then
		table.insert(mapall_chain.rules, 1,
		    { path = conf_target_ld_so, use_orig_path = true,
		      readonly = true }) 

		-- use dynamic libraries from target, 
		-- when executing native binaries!
		emulate_mode_target_ld_so = conf_target_ld_so
	end
	emulate_mode_target_ld_library_path_prefix = conf_target_ld_so_library_path
else
	emulate_mode_target_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	emulate_mode_target_ld_library_path_suffix =
		host_ld_library_path_suffix
end


local exec_policy_target = {
	name = "Target",
	native_app_ld_so = emulate_mode_target_ld_so,
	native_app_ld_so_supports_argv0 = conf_target_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_target_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = target_root,
	native_app_ld_so_supports_nodefaultdirs = conf_target_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_target_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_target_ld_library_path_suffix,

	native_app_locale_path = conf_target_locale_path,
	native_app_gconv_path = conf_target_gconv_path,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}

--
-- For tools: If tools_root is set and libsb2 has been installed there,
-- then dynamic libraries can be used from tools_root (otherwise we'll
-- be using the libs from the host OS)

tools = tools_root
if (not tools) then
	tools = "/"
end

local emulate_mode_tools_ld_so = nil		-- default = not needed
local emulate_mode_tools_ld_library_path_prefix = ""
local emulate_mode_tools_ld_library_path_suffix = ""

if ((tools_root ~= nil) and conf_tools_sb2_installed) then
	if (conf_tools_ld_so ~= nil) then
		-- Ok to use dynamic libraries from tools!
		emulate_mode_tools_ld_so = conf_tools_ld_so
	end
	emulate_mode_tools_ld_library_path_prefix = conf_tools_ld_so_library_path
	if (conf_tools_locale_path ~= nil) then
		-- use locales from tools
		devel_mode_locale_path = conf_tools_locale_path
	end
else
	emulate_mode_tools_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	emulate_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

local exec_policy_tools = {
	name = "Tools",
	native_app_ld_so = emulate_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}


-- Note that the real path (mapped path) is used when looking up rules!
all_exec_policies_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- Tools. at least qemu might be used from there.
		{prefix = tools, exec_policy_name = "Tools"},

                -- the home directory is expected to contain target binaries:
                {prefix = sbox_user_home_dir, exec_policy_name = "Target"},

		-- Target binaries:
		{prefix = target_root, exec_policy_name = "Target"},

		-- the place where the session was created is expected
		-- to contain target binaries:
		{prefix = sbox_workdir, exec_policy_name = "Target"},

		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy_name = "Default"}
	}
}

exec_policy_chains = {
	all_exec_policies_chain
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_target,
	exec_policy_tools,
	default_exec_policy,
}

