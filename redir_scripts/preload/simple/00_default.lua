-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.



qemu_chain = {
	next_chain = nil,
	binary = ".*qemu.*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/local/lib.*", map_to = "="},
		{path = ".*", map_to = nil}
	}
}

dpkg_chain = {
	next_chain = simple_chain,
	binary = ".*",
	rules = {
		{path = "^/var/dpkg.*", map_to = "="}
	}
}

simple_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/lib/perl.*", map_to = nil},
		{path = "^/usr/lib/dpkg.*", map_to = nil},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/include.*", map_to = "="},
		{path = "^/var/.*/apt.*", map_to = "="},
		{path = "^/var/.*/dpkg.*", map_to = "="},
		{path = "^/host_usr", map_to = "="},
		{path = ".*", map_to = nil}
	}
}

-- fakeroot needs this
sh_chain = {
	next_chain = simple_chain,
	binary = ".*sh.*",
	rules = {
		{path = "^/usr/lib.*", map_to = nil},
	}
}

export_chains = {
	qemu_chain,
	sh_chain,
	simple_chain
}
