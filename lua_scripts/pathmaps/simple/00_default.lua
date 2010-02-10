-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.
--
-- "simple" mode, to be used for software development & building
-- (as the name says, this is the simple solution; See/use the "devel"
-- mode when a more full-featured environment is needed)

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "24"
----------------------------------

tools = tools_root
if (not tools) then
	tools = "/"
end

simple_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- -----------------------------------------------
		-- 1. The session directory
		{dir = session_dir, use_orig_path = true},

		-- -----------------------------------------------
		-- 2. Development environment special destinations:

		{prefix = "/sb2/wrappers",
		 replace_by = sbox_dir.."/share/scratchbox2/wrappers",
		 readonly = true},

		{prefix = "/sb2/scripts",
		 replace_by = sbox_dir.."/share/scratchbox2/scripts",
		 readonly = true},

		{prefix = sbox_user_home_dir, use_orig_path = true},

		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		{path = "/usr/bin/sb2-show",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 99. Other rules.
		{prefix = "/usr/lib/perl", map_to = tools},
		{prefix = "/usr/lib/gcc", map_to = tools},
		{prefix = "/usr/lib", map_to = target_root},
		{prefix = "/usr/include", map_to = target_root},
		{prefix = "/home/user", map_to = target_root},
		{prefix = "/home", use_orig_path = true},
		{prefix = "/host_usr", map_to = target_root},
		{prefix = "/tmp", use_orig_path = true},
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

		--
		-- Following 3 rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolv.conf", force_orig_path = true,
		 readonly = true},

		{prefix = "/lib", map_to = target_root},
		--

		{prefix = tools, use_orig_path = true},
		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = tools}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = basename(sbox_cputransparency_method),
	rules = {
		{prefix = "/usr/lib", map_to = target_root},
		{prefix = "/usr/local/lib", map_to = target_root},
		{prefix = "/tmp", use_orig_path = true},
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

		--
		-- Following 3 rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolv.conf", force_orig_path = true,
		 readonly = true},

		{prefix = "/lib", map_to = target_root},
		--

		{prefix = tools, use_orig_path = true},
		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = tools}
	}
}

export_chains = {
	qemu_chain,
	simple_chain
}

-- Exec policy rules.

-- If the permission token exists and contains "root",
-- use fakeroot
local fakeroot_ld_preload = ""
if sb.get_session_perm() == "root" then
	fakeroot_ld_preload = ":"..host_ld_preload_fakeroot
end

default_exec_policy = {
	name = "Default",

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,

	native_app_ld_library_path_prefix = 
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2,
	native_app_ld_library_path_suffix = host_ld_library_path_suffix,
}

-- Note that the real path (mapped path) is used when looking up rules!
all_exec_policies_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy = default_exec_policy}
	}
}

exec_policy_chains = {
	all_exec_policies_chain
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	default_exec_policy,
}
