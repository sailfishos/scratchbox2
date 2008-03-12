-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{match = ".*qemu.*", use_orig_path = true},
		{prefix = target_root, use_orig_path = true},
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/tmp", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},
		{prefix = os.getenv("HOME") .. "/.scratchbox2",
		 use_orig_path = true},
		{prefix = os.getenv("SBOX_DIR") .. "/share/scratchbox2",
		 use_orig_path = true},

		{prefix = "/etc/resolv.conf", use_orig_path = true},

		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = target_root}
	}
}

export_chains = {
	mapall_chain
}
