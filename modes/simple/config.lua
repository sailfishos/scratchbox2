-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "simple" mode

enable_cross_gcc_toolchain = true

-- Note that the real path (mapped path) is used when looking up rules!
exec_policy_selection = {
		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy_name = "Default"}
}

