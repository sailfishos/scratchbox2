-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "obs-deb-install" mode
-- (based on "obs-rpm-install")

-- ***************************************************
-- NOTE: This is experimental, untested mapping mode!!
-- ***************************************************

enable_cross_gcc_toolchain = false


-- Note that the real path (mapped path) is used when looking up rules!
exec_policy_selection = {
		-- Target binaries:
		{prefix = target_root, exec_policy_name = "Target"},

		-- Tools. at least qemu might be used from there.
		-- Rule isn't active if tools_root is not set.
		{prefix = tools_root, exec_policy_name = "Tools"},

                -- the toolchain, if not from Tools:
                {dir = sbox_target_toolchain_dir, exec_policy_name = "Toolchain"},

                -- the home directory is expected to contain target binaries:
                {dir = sbox_user_home_dir, exec_policy_name = "Target"},

		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy_name = "Host"}
}
