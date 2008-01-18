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
		{prefix = "/lib", map_to = target_root},
		{prefix = "/usr/share/osso", map_to = target_root},
		{prefix = "/usr/lib/perl", map_to = tools_root},
		{prefix = "/usr/lib/dpkg", map_to = tools_root},
		{prefix = "/usr/lib/apt", map_to = tools_root},
		{prefix = "/usr/lib/cdbs", map_to = tools_root},
		{prefix = "/usr/lib/libfakeroot", map_to = tools_root},
		{prefix = "/usr/lib", map_to = target_root},
		{prefix = "/usr/include", map_to = target_root},
		{prefix = "/var/lib/apt", map_to = target_root},
		{prefix = "/var/cache/apt", map_to = target_root},
		{prefix = "/var/lib/dpkg", map_to = target_root},
		{prefix = "/var/cache/dpkg", map_to = target_root},
		{prefix = "/home/user", map_to = target_root},
		{prefix = "/home", map_to = nil},
		{prefix = "/host_usr", map_to = target_root},
		{prefix = "/tmp", map_to = nil},
		{prefix = "/dev", map_to = nil},
		{prefix = "/proc", map_to = nil},
		{prefix = "/sys", map_to = nil},
		{prefix = "/etc/resolv.conf", map_to = nil},
		{prefix = "/etc/apt", map_to = target_root},
		{prefix = tools, map_to = nil},
		{path = "/", map_to = nil},
		{prefix = "/", map_to = tools_root}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = basename(os.getenv("SBOX_CPUTRANSPARENCY_METHOD")),
	rules = {
		{prefix = "/lib", map_to = target_root},
		{prefix = "/usr/lib", map_to = target_root},
		{prefix = "/usr/local/lib", map_to = target_root},
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


export_chains = {
	qemu_chain,
	simple_chain
}
