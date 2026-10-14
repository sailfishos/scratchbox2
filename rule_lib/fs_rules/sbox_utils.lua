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

local sbox_internal_rules = {
	-- Scratchbox directory, always host
	{dir = sbox_dir .. "/share/scratchbox2",
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


		-- gdb wants to have access to our dynamic linker also,
		-- /usr/lib/libsb2/wrappers/*, etc.
		{dir = "/usr/lib/libsb2", use_orig_path = true,
		 protection = readonly_fs_always},
		-- same for lib64
		{dir = "/usr/lib64/libsb2", use_orig_path = true,
		 protection = readonly_fs_always},


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

}

return sbox_internal_rules
