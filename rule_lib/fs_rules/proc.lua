-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2012 Nokia Corporation.
-- License: MIT.

-- Rule file interface version, mandatory.
--
fs_rule_lib_interface_version = "105"
----------------------------------

-- /proc rules.

rule_lib_proc_rules = {
	-- We can't change times or attributes of host's /proc,
	-- but must pretend to be able to do so. Redirect the path
	-- to an existing, dummy location.
	{path = "/proc",
	 func_class = FUNC_CLASS_SET_TIMES,
	 set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

	-- The links under this directory are not real symbolic links. It is an
	-- error to use the path that these symlinks resolve to. It is necessay to
	-- use the original /proc/self/fd/<n> path directly.
	{dir = "/proc/self/fd", force_orig_path = true},

	-- Default:
	{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
	 virtual_path = true},
}		 

return rule_lib_proc_rules

