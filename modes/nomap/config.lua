-- Common config for the "nomap" mode
--
-- SPDX-FileCopyrightText:  Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- SPDX-License-Identifier: MIT

enable_cross_gcc_toolchain = false

-- Note that the real path (mapped path) is used when
-- selecting the exec policy!
exec_policy_selection = {
	{prefix = "/", exec_policy_name = "Default"}
}
