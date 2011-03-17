-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "100"
----------------------------------

sb1_compat_dir = sbox_target_root .. "/scratchbox1-compat"
--
-- scratchbox1-compat is symlink that points to
-- the real sb1 compat directory.  To avoid mapping
-- problems later, we resolve this symlink right now.
--
local resolved_compat_dir = sb.readlink(sb1_compat_dir)
if resolved_compat_dir ~= nil then
	sb1_compat_dir = resolved_compat_dir
end

-- Don't map the working directory where sb2 was started, unless
-- that happens to be the root directory.
if sbox_workdir == "/" then
	-- FIXME. There should be a way to skip a rule...
	unmapped_workdir = "/XXXXXX" 
else
	unmapped_workdir = sbox_workdir
end

-- use "==" to test options as long as there is only one possible option,
-- string.match() is slow..
if sbox_mode_specific_options == "use-global-tmp" then
	tmp_dir_dest = "/tmp"
else
	tmp_dir_dest = session_dir .. "/tmp"
end

-- If the permission token exists and contains "root", target_root
-- will be available in R/W mode. Otherwise it will be "mounted" R/O.
local target_root_is_readonly
if sb.get_session_perm() == "root" then
	target_root_is_readonly = false
else
	target_root_is_readonly = true
end

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

test_first_usr_bin_default_is_bin__replace = {
	{ if_exists_then_replace_by = target_root.."/usr/bin", readonly = true },
	{ replace_by = target_root.."/bin", readonly = true }
}

-- Path == "/":
rootdir_rules = {
		-- Special case for /bin/pwd: Some versions don't use getcwd(),
		-- but instead the use open() + fstat() + fchdir() + getdents()
		-- in a loop, and that fails if "/" is mapped to target_root.
		{path = "/", binary_name = "pwd", use_orig_path = true},

		-- All other programs:
		{path = "/", func_name = ".*stat.*",
                    map_to = target_root, readonly = target_root_is_readonly },
		{path = "/", func_name = ".*open.*",
                    map_to = target_root, readonly = target_root_is_readonly },
		{path = "/", use_orig_path = true},
}

-- "dpkg" needs special processing for some files:
-- we don't want to access wrapped launcher scripts
-- because we might be installing (over) them.
-- This is used for e.g.
--     /etc/osso-af-init/dbus-systembus.sh
--     /usr/bin/scratchbox-launcher.sh
emulate_mode_map_to_sb1compat_unless_dpkg = {
	rules = {
		{ prefix = "/",
		  binary_name = "dpkg",
		  map_to = target_root,
		  readonly = target_root_is_readonly },
		--
		-- All other programs than dpkg:
		{ prefix = "/",
		  map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
	}
}

emulate_mode_rules_usr = {
	rules = {
		-- gdb wants to have access to our dynamic linker also.
		{path = "/usr/lib/libsb2/ld-2.5.so", use_orig_path = true,
		readonly = true},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 readonly = true},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, readonly = true},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 readonly = true},

		{ path = "/usr/bin/scratchbox-launcher.sh",
                  rules = emulate_mode_map_to_sb1compat_unless_dpkg},

		{dir = "/usr", map_to = target_root,
		readonly = target_root_is_readonly}
	}
}

emulate_mode_rules_etc = {
	rules = {
                { path = "/etc/osso-af-init/dbus-systembus.sh",
                  rules = emulate_mode_map_to_sb1compat_unless_dpkg},

		-- Following rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{path = "/etc/resolv.conf", force_orig_path = true,
		 readonly = true},

		{dir = "/etc", map_to = target_root,
		 readonly = target_root_is_readonly}
	}
}

emulate_mode_rules_var = {
	rules = {
		-- Following rule are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		readonly = true},

		--
		{prefix = "/var/run", map_to = session_dir},

		{dir = "/var", map_to = target_root,
		readonly = target_root_is_readonly}
		}
	}
}

-- /scratchbox or /targets
-- Note that when you add/remove these, check
-- also that the special cases for dpkg match these.
--
emulate_mode_rules_scratchbox1 = {
	rules = {
		{ dir = "/targets", map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},

		-- "policy-rc.d" checks if scratchbox-version exists, 
		-- to detect if it is running inside scratchbox..
		{prefix = "/scratchbox/etc/scratchbox-version",
		 replace_by = "/usr/share/scratchbox2/version",
		 readonly = true, virtual_path = true},

		-- Stupid references to /scratchbox/tools/bin
		-- and /scratchbox/compilers/bin: these should not
		-- be used at all, but if they do, try to guess
		-- where the thing is. In any case, log a warning
		-- and allow only R/O access.
		{prefix = "/scratchbox/tools/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 readonly = true, virtual_path = true},
		{prefix = "/scratchbox/compilers/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		--
		-- Some of the scripts that starts up fremantle
		-- GUI test existense of /scratchbox so we point
		-- it to sb1_compat_dir.
		--
		{dir = "/scratchbox", replace_by = sb1_compat_dir,
		 readonly = true, virtual_path = true},
	}
}

mapall_chain = {
	next_chain = nil,
	rules = {
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 readonly = true},

		{prefix = target_root, use_orig_path = true,
		 readonly = target_root_is_readonly},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 readonly = true},

		{dir = "/scratchbox", chain = emulate_mode_rules_scratchbox1},
		{dir = "/targets", chain = emulate_mode_rules_scratchbox1},

		-- 
		{prefix = "/tmp", replace_by = tmp_dir_dest},

		-- 
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		-- -----------------------------------------------
		{prefix = sbox_user_home_dir, use_orig_path = true},

		-- "user" is a special username, and should be mapped
		-- to target_root
		-- (but note that if the real user name is "user",
		-- our previous rule handled that and this rule won't be used)
		{prefix = "/home/user", map_to = target_root,
		 readonly = target_root_is_readonly},

		-- Other home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},
		-- -----------------------------------------------

		-- The default is to map everything to target_root,
		-- except that we don't map the directory tree where
		-- sb2 was started.
		{prefix = unmapped_workdir, use_orig_path = true},

		{dir = "/usr", chain = emulate_mode_rules_usr},
		{dir = "/etc", chain = emulate_mode_rules_etc},
		{dir = "/var", chain = emulate_mode_rules_var},

		{path = "/", chain = rootdir_rules},
		{prefix = "/", map_to = target_root,
		 readonly = target_root_is_readonly}
	}
}

-- This allows access to tools with full host paths,
-- this is needed for example to be able to
-- start CPU transparency from tools.
-- Used only when tools_root is set.
local tools_chain = {
	next_chain = mapall_chain,
	rules = {
		{dir = tools_root, use_orig_path = true},
	},
}

-- do not try to remap files from this table at all
override_nomap = {
	os.getenv("SSH_AUTH_SOCK"),
}

if (tools_root ~= nil) and (tools_root ~= "/") then
        -- Tools root is set.
        export_chains = {
                tools_chain,
                mapall_chain
        }
else
        -- No tools_root.
        export_chains = {
                mapall_chain
        }
end

