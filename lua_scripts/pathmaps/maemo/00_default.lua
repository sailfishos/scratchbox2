-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

tools = tools_root
if (not tools) then
	tools = "/"
end

simple_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{prefix = "/lib", map_to = "="},
		{prefix = "/usr/share/osso", map_to = "="},
		{prefix = "/usr/lib/perl", map_to = tools_root},
		{prefix = "/usr/lib/dpkg", map_to = tools_root},
		{prefix = "/usr/lib/apt", map_to = tools_root},
		{prefix = "/usr/lib/cdbs", map_to = tools_root},
		{prefix = "/usr/lib", map_to = "="},
		{prefix = "/usr/include", map_to = "="},
		{prefix = "/var/lib/apt", map_to = "="},
		{prefix = "/var/cache/apt", map_to = "="},
		{prefix = "/var/lib/dpkg", map_to = "="},
		{prefix = "/var/cache/dpkg", map_to = "="},
		{prefix = "/home/user", map_to = "="},
		{prefix = "/home", map_to = nil},
		{prefix = "/host_usr", map_to = "="},
		{prefix = "/tmp", map_to = nil},
		{prefix = "/dev", map_to = nil},
		{prefix = "/proc", map_to = nil},
		{prefix = "/sys", map_to = nil},
		{prefix = "/etc/resolv.conf", map_to = nil},
		{prefix = "/etc/apt", map_to = "="},
		{prefix = tools, map_to = nil},
		{path = "/", map_to = nil},
		{prefix = "/", map_to = tools_root}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = basename(os.getenv("SBOX_CPUTRANSPARENCY_METHOD")),
	rules = {
		{prefix = "/lib", map_to = "="},
		{prefix = "/usr/lib", map_to = "="},
		{prefix = "/usr/local/lib", map_to = "="},
		{prefix = "/tmp", map_to = nil},
		{prefix = "/dev", map_to = nil},
		{prefix = "/proc", map_to = nil},
		{prefix = "/sys", map_to = nil},
		{prefix = "/etc/resolv.conf", map_to = nil},
		{prefix = tools, map_to = nil},
		{path = "/", map_to = nil},
		{prefix = "/", map_to = tools_root}
	}
}


-- fakeroot needs this
sh_chain = {
	next_chain = simple_chain,
	binary = "sh",
	rules = {
		{match = "/usr/lib.*la", map_to = "="},
		{prefix = "/usr/lib", map_to = tools_root}
	}
}

export_chains = {
	qemu_chain,
	sh_chain,
	simple_chain
}
