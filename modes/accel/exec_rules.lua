-- Author: Lauri T. Aarnio
-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (c) 2009 Nokia Corporation.
-- License: MIT.
--
-- "accel" mode = build accelerator mode, to be used for software 
-- development & building when the rootstrap and the tools are "twins":
-- Built from the same sources, but tools contains native binaries while
-- the rootstrap contains target binaries.


-- Rule file interface version, mandatory.
--
rule_file_interface_version = "203"
----------------------------------

tools = tools_root
if (not tools) then
	tools = "/"
end

sb2_share_dir = sbox_user_home_dir.."/.scratchbox2/"..sbox_target.."/share"

-- =========== Exec policies:  ===========

--
-- For tools: If tools_root is set and libsb2 has been installed there,
-- then dynamic libraries can be used from tools_root (otherwise we'll
-- be using the libs from the host OS)

devel_mode_tools_ld_so = nil		-- default = not needed
devel_mode_tools_ld_library_path_prefix = ""
devel_mode_tools_ld_library_path_suffix = ""
-- localization support for tools
devel_mode_locale_path = nil
devel_mode_gconv_path = nil

if ((tools_root ~= nil) and conf_tools_sb2_installed) then
	if (conf_tools_ld_so ~= nil) then
		-- Ok to use dynamic libraries from tools!
		devel_mode_tools_ld_so = conf_tools_ld_so
	end
	devel_mode_tools_ld_library_path_prefix = conf_tools_ld_so_library_path
	if (conf_tools_locale_path ~= nil) then
		-- use locales from tools
		devel_mode_locale_path = conf_tools_locale_path
		devel_mode_gconv_path = conf_tools_gconv_path
	end
else
	devel_mode_tools_ld_library_path_prefix =
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	devel_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

tools_script_interp_rules = {
		{dir = "/scratchbox/tools/bin",
		 replace_by = tools .. "/usr/bin", log_level = "warning"},

		{prefix = "/", map_to = tools}
}

exec_policy_tools = {
	name = "Tools",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

exec_policy_tools_perl = {
	name = "Tools-perl",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (t.p)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

exec_policy_tools_python = {
	name = "Tools-python",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (t.p)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

-- For target binaries:
-- First, note that "foreign" binaries are easy to handle, no problem there.
-- But if CPU transparency method has not been set, then host CPU == target CPU:
-- we have "target's native" and "host's native" binaries, that would look 
-- identical (and valid!) to the kernel. But they need to use different 
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

devel_mode_target_ld_so = nil		-- default = not needed
devel_mode_target_ld_library_path_prefix = ""
devel_mode_target_ld_library_path_suffix = nil

if (conf_target_sb2_installed) then
	if (conf_target_ld_so ~= nil) then
		-- use dynamic libraries from target, 
		-- when executing native binaries!
		devel_mode_target_ld_so = conf_target_ld_so

		-- FIXME: This exec policy should process (map components of)
		-- the current value of LD_LIBRARY_PATH, and add the results
		-- to devel_mode_target_ld_library_path just before exec.
		-- This has not been done yet.
	end
	devel_mode_target_ld_library_path_prefix = conf_target_ld_so_library_path
else
	devel_mode_target_ld_library_path_prefix =
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	devel_mode_target_ld_library_path_suffix =
		host_ld_library_path_suffix
end

exec_policy_target = {
	name = "Target",
	native_app_ld_so = devel_mode_target_ld_so,
	native_app_ld_so_supports_argv0 = conf_target_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_target_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = target_root,
	native_app_ld_so_supports_nodefaultdirs = conf_target_ld_so_supports_nodefaultdirs,

	native_app_locale_path = conf_target_locale_path,

	native_app_ld_preload_prefix = host_ld_preload,

	native_app_ld_library_path_prefix = devel_mode_target_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_target_ld_library_path_suffix,
}

--
-- For real host binaries:

exec_policy_host_os = {
	name = "Host",
	log_level = "debug",
	log_message = "executing in host OS mode",

	script_interpreter_rules = {
			{prefix = "/", use_orig_path = true}
	},

	-- native_app_ld_library_path* can be left undefined,
	-- host settings will be used (but then it won't add user's
	-- LD_LIBRARY_PATH, which is exactly what we want).
	-- native_app_ld_preload* is not set because of the same reason.
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_host_os,
	exec_policy_target,
	exec_policy_tools,
	exec_policy_tools_perl,
	exec_policy_tools_python,
}

