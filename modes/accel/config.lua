-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- License: MIT.
--
-- Common config for the "accel" mode

enable_cross_gcc_toolchain = true

tools = tools_root
if (not tools) then
	tools = "/"
end
--
-- Note that the real path (mapped path) is used here:
exec_policy_selection = {
	-- Tools:
	-- (tools must be listed first, the tools directory
	-- might be under user's home directory)
	{prefix = tools .. "/usr/bin/perl",
	 exec_policy_name = "Tools-perl"},
	{prefix = tools .. "/usr/bin/python",
	 exec_policy_name = "Tools-python"},
	{prefix = tools, exec_policy_name = "Tools"},

	-- ~/bin probably contains programs for the host OS:
	{prefix = sbox_user_home_dir.."/bin",
	 exec_policy_name = "Host"},

	-- Other places under the home directory are expected
	-- to contain target binaries:
	{prefix = sbox_user_home_dir, exec_policy_name = "Target"},

	-- Target binaries:
	{prefix = target_root, exec_policy_name = "Target"},

	-- the place where the session was created is expected
	-- to contain target binaries:
	{prefix = sbox_workdir, exec_policy_name = "Target"},

	-- -----------------------------------------------
	-- DEFAULT RULE (must exist):
	{prefix = "/", exec_policy_name = "Host"}
}

