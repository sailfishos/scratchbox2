-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

sb1_compat_dir = sbox_target_root .. "/scratchbox1-compat"

-- Don't map the working directory where sb2 was started, unless
-- that happens to be the root directory.
if sbox_workdir == "/" then
	-- FIXME. There should be a way to skip a rule...
	unmapped_workdir = "/XXXXXX" 
else
	unmapped_workdir = sbox_workdir
end

mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{match = ".*qemu.*", use_orig_path = true},
		{prefix = target_root, use_orig_path = true},

		-- Scratchbox 1 compatibility rules:
		{ prefix = "/targets/", map_to = sb1_compat_dir },
		{ path = "/usr/bin/scratchbox-launcher.sh",
                    map_to = sb1_compat_dir },
                { path = "/etc/osso-af-init/dbus-systembus.sh",
                    map_to = sb1_compat_dir },
		
		-- 
		{prefix = session_dir, use_orig_path = true},
		{prefix = "/tmp", map_to = session_dir},

		-- 
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},
		{prefix = sbox_user_home_dir .. "/.scratchbox2",
		 use_orig_path = true},
		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		{prefix = "/etc/resolv.conf", use_orig_path = true},

		-- The default is to map everything to target_root,
		-- execpt that we don't map the directory tree where
		-- sb2 was started.
		{prefix = unmapped_workdir, use_orig_path = true},
		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = target_root}
	}
}

export_chains = {
	mapall_chain
}
