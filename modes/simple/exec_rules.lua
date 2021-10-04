-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.
--
-- "simple" mode, to be used for software development & building
-- (as the name says, this is the simple solution; See/use the "devel"
-- mode when a more full-featured environment is needed)

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "203"
----------------------------------

tools = tools_root
if (not tools) then
	tools = "/"
end

-- Exec policy rules.


default_exec_policy = {
	name = "Default",

	native_app_ld_preload_prefix = host_ld_preload,

	native_app_ld_library_path_prefix = 
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2,
	native_app_ld_library_path_suffix = host_ld_library_path_suffix,
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	default_exec_policy,
}

