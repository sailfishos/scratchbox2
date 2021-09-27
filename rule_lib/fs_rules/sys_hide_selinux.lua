-- Copyright (C) 2012 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
fs_rule_lib_interface_version = "105"
----------------------------------

-- /sys rules which are needed for hiding selinux functionality:
-- If the host has selinux enabled, the programs running inside
-- SB2's sessions should not be able to detect that. Usually SB2
-- is configured to simulate a host without selinux functions.
-- For example, groupadd (or useradd) may fail if they think
-- that selinux is active but then don't have the necessary
-- privileges (as they don't; processes inside the sessions
-- may be running with minimal privileges within SB)

rules_sys = {

	-- don't try to set timestamps on real directory /sys,
	-- redirect to a private location
	-- (some rpm tools fail if the call fails)
	{name = "rule lib: /sys (set times)",
	 path = "/sys",
	 func_class = FUNC_CLASS_SET_TIMES,
	 set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

	-- Hide selinux
	{name = "rule lib: /sys/fs/selinux",
	 dir = "/sys/fs/selinux", map_to = target_root},

	-- The default: Use real /sys
	{name = "rule lib: /sys/* default rule",
	 dir = "/sys", use_orig_path = true},
}

return rules_sys

