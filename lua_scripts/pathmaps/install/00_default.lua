-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2008 Movial
-- Licensed under MIT license.

if (tools_root and tools_root ~= "/") then
	tools_source = tools_root
	tools_target = tools_root
else
	tools_source = "/nonexistent"
	tools_target = "/"
end

sb2_session_dir = os.getenv("SBOX_SESSION_DIR")
if (not sb2_session_dir) then
	sb2_session_dir = "/tmp"
end

interp_wrapper = os.getenv("SBOX_DIR") .. "/bin/sb2-interp-wrapper"

default_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{ prefix = tools_source, use_orig_path = true, readonly = true },

		{ path = "/bin/sh",   func_name = ".*exec.*", replace_by = interp_wrapper },
		{ path = "/bin/bash", func_name = ".*exec.*", replace_by = interp_wrapper },

		{ prefix = "/bin",           func_name = ".*exec.*", map_to = tools_target },
		{ prefix = "/usr/bin",       func_name = ".*exec.*", map_to = tools_target },
		{ prefix = "/usr/local/bin", func_name = ".*exec.*", map_to = tools_target },

		{ prefix = "/dev", func_name = "open.*", use_orig_path = true },
		{ prefix = "/proc", use_orig_path = true },
		{ prefix = "/sys", use_orig_path = true },

		{ prefix = sb2_session_dir, use_orig_path = true },
		{ prefix = "/tmp", map_to = sb2_session_dir },

		{ prefix = os.getenv("HOME"), use_orig_path = true },
		{ prefix = os.getenv("SBOX_WORKDIR"), use_orig_path = true },
		{ prefix = os.getenv("SBOX_DIR") .. "/share/scratchbox2", use_orig_path = true, readonly = true },
		{ prefix = os.getenv("SBOX_DIR") .. "/bin", use_orig_path = true, readonly = true },
		{ prefix = os.getenv("SBOX_TARGET_TOOLCHAIN_DIR"), use_orig_path = true, readonly = true },

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
