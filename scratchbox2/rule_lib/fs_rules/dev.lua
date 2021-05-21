-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2012 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
fs_rule_lib_interface_version = "105"
----------------------------------

-- /dev rules.

rules_dev = {
	-- FIXME: This rule should have "protection = eaccess_if_not_owner_or_root",
	-- but that kind of protection is not yet supported.

	-- ==== Blacklisted targets: ====
	-- Some real device nodes (and other objects in /dev)
	-- should never be accessible from the scratchbox'ed session.
	-- Redirect to a session-specific directory.
	{name = "rule lib: /dev/initctl",
	 path = "/dev/initctl",
	 map_to = session_dir, protection = readonly_fs_if_not_root },
	-- ==== End of Blacklist ====

	{name = "rule lib: /dev/shm (stat)",
	 path = "/dev/shm",
	 func_class = FUNC_CLASS_STAT, use_orig_path = true},

	{name = "rule lib: /dev/shm/*",
	 dir = "/dev/shm",
	 replace_by = session_dir .. "/tmp"},

	-- We can't change times or attributes of host's devices,
	-- but must pretend to be able to do so. Redirect the path
	-- to an existing, dummy location.
	{name = "rule lib: /dev/* (set times)",
	 dir = "/dev",
	 func_class = FUNC_CLASS_SET_TIMES,
	 set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

	-- The directory itself.
	{name = "rule lib: /dev directory itself",
	 path = "/dev", use_orig_path = true},

	-- If a selected device node needs to be opened with
	-- O_CREAT set, use the real device (e.g. "echo >/dev/null"
	-- does that)
	{name = "rule lib: /dev/console (creat)",
	 path = "/dev/console",
	 func_class = FUNC_CLASS_CREAT, use_orig_path = true},
	{name = "rule lib: /dev/null (creat)",
	 path = "/dev/null", 
	 func_class = FUNC_CLASS_CREAT, use_orig_path = true},
        {name = "rule lib: /dev/full (creat)",
	 path = "/dev/full",
	 func_class = FUNC_CLASS_CREAT, use_orig_path = true},
	{name = "rule lib: /dev/tty* (creat)",
	 prefix = "/dev/tty", 
	 func_class = FUNC_CLASS_CREAT, use_orig_path = true},
	{name = "rule lib: /dev/fb* (creat)",
	 prefix = "/dev/fb", 
	 func_class = FUNC_CLASS_CREAT, use_orig_path = true},

	-- mknod is simulated. Redirect to a directory where
	-- mknod can create the node.
	-- Also, typically, rename() is used to rename nodes created by
	-- mknod() (and it can't be used to rename real devices anyway).
	-- It must be possible to create symlinks and files in /dev, too.
	{name = "rule lib: /dev/* (mknod,rename,symlink,creat)",
	 dir = "/dev",
	 func_class = FUNC_CLASS_MKNOD + FUNC_CLASS_RENAME +
		      FUNC_CLASS_SYMLINK + FUNC_CLASS_CREAT,
	 map_to = session_dir, protection = readonly_fs_if_not_root },

	-- Allow removal of simulated nodes, regardless of the name
	-- (e.g. a simulated /dev/null might have been created 
	-- to session_dir, even if it won't be used due to the 
	-- /dev/null rule below)
	{name = "rule lib: /dev/* (remove)",
	 dir = "/dev",
	 func_class = FUNC_CLASS_REMOVE,
	 actions = {
		{ if_exists_then_map_to = session_dir },
		{ use_orig_path = true }
	 },
	},

	-- Default: If a node has been created by mknod, and that was
	-- simulated, use the simulated target.
	-- Otherwise use real devices.
	-- However, there are some devices we never want to simulate...
	{name = "rule lib: /dev/console",
	 path = "/dev/console", use_orig_path = true},
	{name = "rule lib: /dev/null",
	 path = "/dev/null", use_orig_path = true},
	{name = "rule lib: /dev/full",
	 path = "/dev/full", use_orig_path = true},
	{name = "rule lib: /dev/tty*",
	 prefix = "/dev/tty", use_orig_path = true},
	{name = "rule lib: /dev/fb*",
	 prefix = "/dev/fb", use_orig_path = true},

	{name = "rule lib: /dev/* default rule",
	 dir = "/dev", actions = {
			{ if_exists_then_map_to = session_dir },
			{ use_orig_path = true }
		},
	},
}

return rules_dev

