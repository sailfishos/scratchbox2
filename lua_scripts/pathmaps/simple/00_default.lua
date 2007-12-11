-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.


simple_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/bin/xml2.conf.*", map_to = "="},
		{path = "^/usr/bin/gobject.query.*", map_to = "="},
		{path = "^/usr/lib/perl.*", map_to = nil},
		{path = "^/usr/lib/dpkg.*", map_to = nil},
		{path = "^/usr/lib/cdbs.*", map_to = nil},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/include.*", map_to = "="},
		{path = "^/var/.*/apt.*", map_to = "="},
		{path = "^/var/.*/dpkg.*", map_to = "="},
		{path = "^/host_usr", map_to = "="},
		{path = ".*", map_to = nil}
	}
}

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
		{path = "^/var/dpkg.*", map_to = "="},
		{path = "^/var/lib/dpkg.*", map_to = "="}
	}
}

apt_chain = {
	next_chain = simple_chain,
	binary = ".*apt.*",
	rules = {
		{path = "^" .. escape_string(target_root) .. ".*", map_to = nil},
		{path = "^/var/lib/apt.*", map_to = "="},
		{path = "^/var/cache/apt.*", map_to = "="},
		{path = "^/usr/lib/apt.*", map_to = nil},
		{path = "^/etc/apt.*", map_to = "="}
	}
}


-- fakeroot needs this
sh_chain = {
	next_chain = simple_chain,
	binary = ".*sh.*",
	rules = {
		{path = "^/usr/lib.*la", map_to = "="},
		{path = "^/usr/lib.*", map_to = nil},
	}
}

export_chains = {
	qemu_chain,
	sh_chain,
	apt_chain,
	simple_chain
}
