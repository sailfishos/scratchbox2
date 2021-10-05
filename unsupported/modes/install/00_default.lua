-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2008 Movial
-- License: MIT.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "22"
----------------------------------

if (tools_root and tools_root ~= "/") then
	tools_source = tools_root
	tools_target = tools_root
else
	tools_source = "/nonexistent"
	tools_target = "/"
end

interp_wrapper = sbox_dir .. "/bin/sb2-interp-wrapper"

default_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{ dir = session_dir, use_orig_path = true },
		{ prefix = tools_source, use_orig_path = true, readonly = true },

		{ path = "/bin/sh",   func_name = ".*exec.*", replace_by = interp_wrapper },
		{ path = "/bin/bash", func_name = ".*exec.*", replace_by = interp_wrapper },

		{ prefix = "/bin",           func_name = ".*exec.*", map_to = tools_target },
		{ prefix = "/usr/bin",       func_name = ".*exec.*", map_to = tools_target },
		{ prefix = "/usr/local/bin", func_name = ".*exec.*", map_to = tools_target },

		{ prefix = "/dev", func_name = "open.*", use_orig_path = true },
		{ dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{ prefix = "/sys", use_orig_path = true },

		{ prefix = "/tmp", map_to = session_dir },

		{ prefix = sbox_user_home_dir, use_orig_path = true },
		{ prefix = sbox_workdir, use_orig_path = true },
		{ prefix = sbox_dir .. "/share/scratchbox2", use_orig_path = true, readonly = true },
		{ prefix = sbox_dir .. "/bin", use_orig_path = true, readonly = true },
		{ prefix = sbox_target_toolchain_dir, use_orig_path = true, readonly = true },

		{ prefix = "/", map_to = target_root },
	}
}

bash_chain = {
	next_chain = default_chain,
	binary = "bash",
	rules = {
		{ prefix = "/bin",           func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/bin",       func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/local/bin", func_name = "__xstat.*", map_to = tools_target },
	}
}

sh_chain = {
	next_chain = default_chain,
	binary = "sh",
	rules = {
		{ prefix = "/bin",           func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/bin",       func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/local/bin", func_name = "__xstat.*", map_to = tools_target },
	}
}

interp_wrapper_chain = {
	next_chain = default_chain,
	binary = "sb2-interp-wrapper",
	rules = {
		{ prefix = "/bin",           func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/bin",       func_name = "__xstat.*", map_to = tools_target },
		{ prefix = "/usr/local/bin", func_name = "__xstat.*", map_to = tools_target },
	}
}

export_chains = {
	sh_chain,
	bash_chain,
	interp_wrapper_chain,
	default_chain,
}

-- Exec policy rules.

default_exec_policy = {
	name = "Default"
}

-- Note that the real path (mapped path) is used when looking up rules!
all_exec_policies_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy = default_exec_policy}
	}
}

exec_policy_chains = {
	all_exec_policies_chain
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	default_exec_policy,
}

