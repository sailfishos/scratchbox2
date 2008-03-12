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
		{prefix = "/home", use_orig_path = true},
		{prefix = "/host_usr", map_to = target_root},
		{prefix = "/tmp", use_orig_path = true},
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},
		{prefix = "/etc/resolv.conf", use_orig_path = true},
		{prefix = "/etc/apt", map_to = target_root},
		{prefix = tools, use_orig_path = true},
		{path = "/", use_orig_path = true},
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
		{prefix = "/tmp", use_orig_path = true},
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},
		{prefix = "/etc/resolv.conf", use_orig_path = true},
		{prefix = tools, use_orig_path = true},
		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = tools_root}
	}
}


export_chains = {
	qemu_chain,
	simple_chain
}
