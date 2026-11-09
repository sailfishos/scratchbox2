--[[
	SPDX-License-Identifier: GPL-2.0-or-later
	SPDX-FileCopyrightText: 2026 Björn Kettunen
--]]

--[[ Rule file interface version, mandatory.
]]--
fs_rule_lib_interface_version = "105"
----------------------------------

--[[
	Scratchbox file system rules for internal directories and utilities
]]--
local test_first_target_then_host_default_is_target = {
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ if_exists_then_map_to = "/", protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always }
}

local test_first_tools_then_target_default_is_tools = {
	{ if_exists_then_map_to = tools, protection = readonly_fs_always },
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ map_to = tools, protection = readonly_fs_always }
}

local  libsb2_exec_policy_test = {
	{ if_active_exec_policy_is = "Target",
	  map_to = target_root, protection = readonly_fs_always },
	{ if_active_exec_policy_is = "Tools",
	  map_to = tools, protection = readonly_fs_always},
	{ if_active_exec_policy_is = "Tools-Perl",
	  map_to = tools, protection = readonly_fs_always},
	{ if_active_exec_policy_is = "Tools-Python",
	  map_to = tools, protection = readonly_fs_always},
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, protection = readonly_fs_always},
	{ map_to = target_root, protection = readonly_fs_always }
}


local sbox_internal_rules = {
	-- First paths that should never be mapped:
	{dir = session_dir, use_orig_path = true},

	-- -----------------------------------------------
	-- 1. General SB2 environment:
	-- Scratchbox directory, always host
	{prefix = sbox_dir .. "/share/scratchbox2",
	 use_orig_path = true, readonly = true},

	{dir = sbox_dir .. "/share/scratchbox2",
	 use_orig_path = true},

	{prefix = sbox_user_home_dir .. "/.scratchbox2",
	 use_orig_path = true},

	-- The real sbox_dir.."/lib/libsb2" must be available:
	--
	-- When libsb2 is installed to target we don't want to map
	-- the path where it is found.  For example gdb needs access
	-- to the library and dynamic linker, and these may be in
	-- target_root, or under sbox_dir.."/lib/libsb2", or
	-- under ~/.scratchbox2.
	{dir = sbox_dir .. "/lib/libsb2",
	 actions = test_first_target_then_host_default_is_target},
	{dir = sbox_dir .. "/lib64/libsb2",
	 actions = test_},

	-- gdb wants to have access to our dynamic linker also,
	-- /usr/lib/libsb2/wrappers/*, etc.
	{dir = "/usr/lib/libsb2", use_orig_path = true,
	 protection = readonly_fs_always},
	-- same for lib64
	{dir = "/usr/lib64/libsb2", use_orig_path = true,
	 protection = readonly_fs_always},


	-- ldconfig is static binary, and needs to be wrapped
	-- Gdb needs some special parameters before it
	-- can be run so we wrap it.
	{dir = "/sb2/wrappers",
	 replace_by = session_dir .. "/wrappers." .. active_mapmode,
	 protection = readonly_fs_always},

	-- Scratchbox utilities
	{path = "/usr/bin/sb2-show", use_orig_path = true,
	 protection = readonly_fs_always},
	{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
	 use_orig_path = true, protection = readonly_fs_always},
	{path = "/usr/bin/sb2-session", use_orig_path = true,
	 protection = readonly_fs_always},
	{path = "/usr/bin/sb2-ruletree", use_orig_path = true,
	 protection = readonly_fs_always},
	{path = "/usr/bin/sb2-session", use_orig_path = true,
	 protection = readonly_fs_always},

	--- FIXME: Verify

	{path = sbox_cputransparency_cmd, use_orig_path = true,
	 protection = readonly_fs_always},

	{path = sbox_cputransparency_native_cmd, use_orig_path = true,
	 protection = readonly_fs_always},

	{dir = sbox_target_toolchain_dir, use_orig_path = true,
	 -- don't try to reverse this
	 virtual_path = true,
	 protection = readonly_fs_always},
}

return sbox_internal_rules
