-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

tools = tools_root
if (not tools) then
	tools = "/"
end

simple_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/share/osso.*", map_to = "="},
		{path = "^/usr/lib/perl.*", map_to = tools_root},
		{path = "^/usr/lib/dpkg.*", map_to = tools_root},
		{path = "^/usr/lib/cdbs.*", map_to = tools_root},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/include.*", map_to = "="},
		{path = "^/var/.*/apt.*", map_to = "="},
		{path = "^/var/.*/dpkg.*", map_to = "="},
		{path = "^/home/user.*", map_to = "="},
		{path = "^/home.*", map_to = nil},
		{path = "^/host_usr.*", map_to = "="},
		{path = "^/tmp.*", map_to = nil},
		{path = "^/dev.*", map_to = nil},
		{path = "^/proc.*", map_to = nil},
		{path = "^/sys.*", map_to = nil},
		{path = "^/etc/resolv.conf", map_to = nil},
		{path = "^" .. tools .. ".*", map_to = nil},
		{path = "^/$", map_to = nil},
		{path = ".*", map_to = tools_root}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = ".*qemu.*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/local/lib.*", map_to = "="},
		{path = "^/tmp", map_to = nil},
		{path = "^/dev", map_to = nil},
		{path = "^/proc", map_to = nil},
		{path = "^/sys", map_to = nil},
		{path = "^/etc/resolv.conf", map_to = nil},
		{path = "^" .. tools .. ".*", map_to = nil},
		{path = "^/$", map_to = nil},
		{path = ".*", map_to = tools_root}
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
		{path = "^" .. escape_string(target_root) .. ".*", map_to = tools_root},
		{path = "^/var/lib/apt.*", map_to = "="},
		{path = "^/var/cache/apt.*", map_to = "="},
		{path = "^/usr/lib/apt.*", map_to = tools_root},
		{path = "^/etc/apt.*", map_to = "="}
	}
}


-- fakeroot needs this
sh_chain = {
	next_chain = simple_chain,
	binary = ".*sh.*",
	rules = {
		{path = "^/usr/lib.*la", map_to = "="},
		{path = "^/usr/lib.*", map_to = tools_root}
	}
}

export_chains = {
	qemu_chain,
	sh_chain,
	apt_chain,
	simple_chain
}
