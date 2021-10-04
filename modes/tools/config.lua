-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "tools" mode

enable_cross_gcc_toolchain = false

-- Note that the real path (mapped path) is used when looking up rules!
exec_policy_selection = {
	-- Tools binaries:
	{prefix = tools_root, exec_policy_name = "Tools_root"},

	-- DEFAULT RULE (must exist):
	{prefix = "/", exec_policy_name = "Default"}
}

