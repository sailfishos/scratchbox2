-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2011 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "203"
----------------------------------

-- Exec policy rules.

exec_policy_host = {
	name = "Host",
	native_app_ld_preload_suffix = host_ld_preload_libsb2,
}

exec_policy_toolchain = {
	name = "Toolchain",
	native_app_ld_preload_suffix = host_ld_preload_libsb2,
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
	if (conf_target_ld_so ~= nil) then
		-- use dynamic libraries from target, 
		-- when executing native binaries!
		emulate_mode_target_ld_so = conf_target_ld_so
	end
	emulate_mode_target_ld_library_path_prefix = conf_target_ld_so_library_path
else
	emulate_mode_target_ld_library_path_prefix =
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

	native_app_ld_preload_suffix = host_ld_preload,

	exec_flags = EXEC_FLAGS_FORCE_CPU_TRANSPARENCY,
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
	-- if (conf_tools_locale_path ~= nil) then
	--	-- use locales from tools
	--	devel_mode_locale_path = conf_tools_locale_path
	-- end
else
	emulate_mode_tools_ld_library_path_prefix =
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	emulate_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

tools_script_interp_rules = {
	{prefix = "/", map_to = tools}
}

local exec_policy_tools = {
	name = "Tools",
	native_app_ld_so = emulate_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_tools_ld_library_path_suffix,

	native_app_locale_path = conf_tools_locale_path,
	native_app_gconv_path = conf_tools_gconv_path,

	native_app_ld_preload_suffix = host_ld_preload,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

local exec_policy_tools_perl = {
	name = "Tools-perl",
	native_app_ld_so = emulate_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_suffix = host_ld_preload,

	native_app_locale_path = conf_tools_locale_path,
	native_app_gconv_path = conf_tools_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (perl)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

local exec_policy_tools_python = {
	name = "Tools-python",
	native_app_ld_so = emulate_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_suffix = host_ld_preload,

	native_app_locale_path = conf_tools_locale_path,
	native_app_gconv_path = conf_tools_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (python)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}


-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_target,
	exec_policy_tools,
	exec_policy_toolchain,
	exec_policy_host,
	exec_policy_tools_perl,
	exec_policy_tools_python,
}

