-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "emulate" mode

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

-- Note that the real path (mapped path) is used when looking up rules!
exec_policy_selection = {
		-- Tools. at least qemu might be used from there.
		-- Rule isn't active if tools_root is not set.
		{prefix = tools_root, exec_policy_name = "Tools"},

                -- the home directory is expected to contain target binaries:
                {dir = sbox_user_home_dir, exec_policy_name = "Target"},

		-- Target binaries:
		{prefix = target_root, exec_policy_name = "Target"},

		-- the place where the session was created is expected
		-- to contain target binaries:
		{prefix = sbox_workdir, exec_policy_name = "Target"},

		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy_name = "Host"}
}

