-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.


mapall_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = ".*qemu.*", map_to = nil},
		{path = "^/dev", map_to = nil},
		{path = "^/proc", map_to = nil},
		{path = "^/tmp", map_to = nil},
		{path = "^/sys", map_to = nil},
		{path = ".*", map_to = "="}
	}
}

export_chains = {
	mapall_chain
}
