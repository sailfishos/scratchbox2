-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2007 Nokia Corporation.
-- Licensed under MIT license.

-- "nomap" mapping mode: Does not map any paths anywhere, but still pushes
-- all paths thru SB2's path mapping logic, handles execs, etc.
--
-- This is useful for benchmarking, debugging (SB2's logs are available, if
-- needed), and of course this makes SB2 fully symmetric because now SB2
-- can be used both for cross-compiling and for native builds! :-) ;-)
--
-- Note that the target architecture should be set to host architecture
-- while using this mode; usually a special "nomap" target should be created.
--   Example:
--   for 64-bit intel/amd architectures ("uname -m" displays "x86_64"):
--      sb2-init -A amd64 -M x86_64 -n -m nomap nomap
-- Next, use "sb2 -t nomap" to enter this mode (i.e. things usually go wrong
-- if you try to use the the "-m" option to enter this mode, but the target
-- is still something else than the host. The destination architecture is not
-- selected by the mapping mode...)

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "103"
----------------------------------

fs_mapping_rules = {
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 protection = readonly_fs_always},

		-- Don't map anything else.
		{prefix = "/", force_orig_path = true},
}

