-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "emulate+toolchain" mode

enable_cross_gcc_toolchain = true

-- Note that the real path (mapped path) is used when looking up rules!
exec_policy_selection = {
		-- Tools. at least qemu might be used from there.
		-- Rule isn't active if tools_root is not set.
		{prefix = tools_root, exec_policy_name = "Tools"},

                -- the toolchain, if not from Tools:
                {dir = sbox_target_toolchain_dir, exec_policy_name = "Toolchain"},

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
