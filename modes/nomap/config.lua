-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- License: MIT.
--
-- Common config for the "nomap" mode

enable_cross_gcc_toolchain = false

-- Note that the real path (mapped path) is used when 
-- selecting the exec policy!
exec_policy_selection = {
	{prefix = "/", exec_policy_name = "Default"}
}
