-- Copyright (C) 2008 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2008 Nokia Corporation.
-- Licensed under MIT license.

-- "tools" mapping mode: Almost everything maps to tools_root.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "203"
----------------------------------


-- Exec policy rules.

default_exec_policy = {
	name = "Default",
	native_app_ld_preload_prefix = host_ld_preload,
}

-- For binaries from tools_root:
-- we have "tools' native" and "host's native" binaries, that would look
-- identical (and valid!) to the kernel. But they may need to use different
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

tools_mode_tools_ld_so = nil		-- default = not needed
tools_mode_tools_ld_library_path = nil	-- default = not needed
tools_mode_tools_ld_library_path_prefix = ""
tools_mode_tools_ld_library_path_suffix = nil

if (conf_tools_sb2_installed) then
	if (conf_tools_ld_so ~= nil) then
		-- use dynamic libraries from tools,
		-- when executing native binaries!
		tools_mode_tools_ld_so = conf_tools_ld_so

		-- FIXME: This exec policy should process (map components of)
		-- the current value of LD_LIBRARY_PATH, and add the results
		-- to tools_mode_tools_ld_library_path just before exec.
		-- This has not been done yet.
	end
	tools_mode_tools_ld_library_path_prefix = conf_tools_ld_so_library_path
else
	tools_mode_tools_ld_library_path_prefix =
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	tools_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

local exec_policy_tools = {
	name = "Tools_root",
	native_app_ld_so = tools_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_library_path = tools_mode_tools_ld_library_path,

	native_app_ld_library_path_prefix = tools_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = tools_mode_tools_ld_library_path_suffix,

	native_app_locale_path = conf_tools_locale_path,
	native_app_gconv_path = conf_tools_gconv_path,
	native_app_message_catalog_prefix = conf_tools_message_catalog_prefix,
}


-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_tools,
	default_exec_policy,
}

