-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.


mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{match = ".*qemu.*", map_to = nil},
		{prefix = target_root, map_to = nil},
		{prefix = "/dev", map_to = nil},
		{prefix = "/proc", map_to = nil},
		{prefix = "/tmp", map_to = nil},
		{prefix = "/sys", map_to = nil},
		{prefix = os.getenv("HOME") .. "/.scratchbox2", map_to = nil},
		{prefix = os.getenv("SBOX_DIR") .. "/share/scratchbox2", map_to = nil},

		{match = ".*", map_to = target_root}
	}
}

export_chains = {
	mapall_chain
}
