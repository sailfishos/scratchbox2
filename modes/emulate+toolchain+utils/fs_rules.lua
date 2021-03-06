-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "105"
----------------------------------

sb1_compat_dir = sbox_target_root .. "/scratchbox1-compat"
--
-- scratchbox1-compat is symlink that points to
-- the real sb1 compat directory.  To avoid mapping
-- problems later, we resolve this symlink right now.
--
local resolved_compat_dir = sblib.readlink(sb1_compat_dir)
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
	var_tmp_dir_dest = "/var/tmp"
else
	tmp_dir_dest = session_dir .. "/tmp"
	var_tmp_dir_dest = session_dir .. "/var/tmp"
end

test_first_target_then_host_default_is_target = {
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ if_exists_then_map_to = "/", protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always }
}

test_first_usr_bin_default_is_bin__replace = {
	{ if_exists_then_replace_by = target_root.."/usr/bin", protection = readonly_fs_always },
	{ replace_by = target_root.."/bin", protection = readonly_fs_always }
}

test_first_tools_then_target_default_is_tools = {
	{ if_exists_then_map_to = tools, readonly = true },
	{ if_exists_then_map_to = target_root, readonly = true },
	{ map_to = tools, readonly = true }
}

-- Path == "/":
rootdir_rules = {
		-- Special case for /bin/pwd: Some versions don't use getcwd(),
		-- but instead the use open() + fstat() + fchdir() + getdents()
		-- in a loop, and that fails if "/" is mapped to target_root.
		{path = "/", binary_name = "pwd", use_orig_path = true},

		-- All other programs:
		{path = "/",
		    func_class = FUNC_CLASS_STAT + FUNC_CLASS_OPEN + FUNC_CLASS_SET_TIMES,
                    map_to = target_root, protection = readonly_fs_if_not_root },

		-- Default: Map to real root.
		{path = "/", use_orig_path = true},
}

-- "dpkg" needs special processing for some files:
-- we don't want to access wrapped launcher scripts
-- because we might be installing (over) them.
-- This is used for e.g.
--     /etc/osso-af-init/dbus-systembus.sh
--     /usr/bin/scratchbox-launcher.sh
emulate_mode_map_to_sb1compat_unless_dpkg = {
		{ prefix = "/",
		  binary_name = "dpkg",
		  map_to = target_root,
		  protection = readonly_fs_if_not_root },
		--
		-- All other programs than dpkg:
		{ prefix = "/",
		  map_to = sb1_compat_dir,
		  protection = readonly_fs_if_not_root},
}

emulate_mode_rules_bin = {
		{path = "/bin/sh", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/bash", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/echo", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/bin/cp", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/rm", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/mv", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/ln", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/ls", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/cat", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/egrep", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/grep", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/bin/mkdir", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/bin/rmdir", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/bin/mktemp", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/bin/gzip", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/tar", use_orig_path = true,
		 protection = readonly_fs_always},

		{name = "/bin default rule", dir = "/bin", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr_bin = {
		{path = "/usr/bin/awk", force_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/gawk", force_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/expr", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/find", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/diff", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/cmp", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/tr", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/dirname", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/basename", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/grep", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/egrep", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/make", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/m4", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/bzip2", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/gzip", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sed", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/bison", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/flex", use_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/patch", force_orig_path = true,
		 protection = readonly_fs_always},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, protection = readonly_fs_always},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 protection = readonly_fs_always},

		{ path = "/usr/bin/scratchbox-launcher.sh",
                  rules = emulate_mode_map_to_sb1compat_unless_dpkg},

		-- next, automatically generated rules for /usr/bin:
		{name = "/usr/bin autorules", dir = "/usr/bin", rules = argvmods_rules_for_usr_bin,
		 virtual_path = true}, -- don't reverse these.

		{name = "/usr/bin default rule", dir = "/usr/bin", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr = {
		{name = "/usr/bin branch", dir = "/usr/bin", rules = emulate_mode_rules_usr_bin},

		-- gdb wants to have access to our dynamic linker also,
		-- /usr/lib/libsb2/wrappers/*, etc.
		{dir = "/usr/lib/libsb2", use_orig_path = true,
		 protection = readonly_fs_always},

		{dir = "/usr/lib/gcc", actions = test_first_tools_then_target_default_is_tools},

		-- If a program from tools loads plugins,
		-- they should be dlopened from tools as well.
		-- However, libdir in tools can be different than one in target.
		{dir = "/usr/lib", func_class = FUNC_CLASS_DLOPEN,
		 actions = {
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_replace_by = tools .. "/usr/lib64",
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_map_to = tools,
		   protection = readonly_fs_always},
		   {if_active_exec_policy_is = "Host",
			if_exists_then_replace_by = tools .. "/usr/lib64",
			protection = readonly_fs_always},
		   {if_active_exec_policy_is = "Host",
			if_exists_then_map_to = tools,
			protection = readonly_fs_always},
		  { map_to = target_root, protection = readonly_fs_always },
		 },
		},

		{dir = "/usr", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_etc = {
                { path = "/etc/osso-af-init/dbus-systembus.sh",
                  rules = emulate_mode_map_to_sb1compat_unless_dpkg},

		-- Following rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/etc/resolv.conf", force_orig_path = true,
		 protection = readonly_fs_always},

		{dir = "/etc", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_var = {
		-- Following rule are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		protection = readonly_fs_always},

		--
		{prefix = "/var/run", map_to = session_dir},
		{prefix = "/var/tmp", replace_by = var_tmp_dir_dest},

		{dir = "/var", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

-- /scratchbox or /targets
-- Note that when you add/remove these, check
-- also that the special cases for dpkg match these.
--
emulate_mode_rules_scratchbox1 = {
		{ dir = "/targets", map_to = sb1_compat_dir,
		  protection = readonly_fs_if_not_root},

		-- "policy-rc.d" checks if scratchbox-version exists, 
		-- to detect if it is running inside scratchbox..
		{prefix = "/scratchbox/etc/scratchbox-version",
		 replace_by = "/usr/share/scratchbox2/version",
		 protection = readonly_fs_always, virtual_path = true},

		-- Stupid references to /scratchbox/tools/bin
		-- and /scratchbox/compilers/bin: these should not
		-- be used at all, but if they do, try to guess
		-- where the thing is. In any case, log a warning
		-- and allow only R/O access.
		{prefix = "/scratchbox/tools/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 protection = readonly_fs_always, virtual_path = true},
		{prefix = "/scratchbox/compilers/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 protection = readonly_fs_always, virtual_path = true},

		--
		-- Some of the scripts that starts up fremantle
		-- GUI test existense of /scratchbox so we point
		-- it to sb1_compat_dir.
		--
		{dir = "/scratchbox", replace_by = sb1_compat_dir,
		 protection = readonly_fs_always, virtual_path = true},
}

emulate_mode_rules = {
		-- First paths that should never be mapped:
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 protection = readonly_fs_always},

		{dir = sbox_target_toolchain_dir, use_orig_path = true,
		 protection = readonly_fs_always},

		{prefix = target_root, use_orig_path = true,
		 protection = readonly_fs_if_not_root},

		{path = os.getenv("SSH_AUTH_SOCK"), use_orig_path = true},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 protection = readonly_fs_always},

		-- 
		-- Scratchbox 1 compatibility rules:
		{dir = "/scratchbox", rules = emulate_mode_rules_scratchbox1},
		{dir = "/targets", rules = emulate_mode_rules_scratchbox1},

		-- 
		{prefix = "/tmp", replace_by = tmp_dir_dest},

		{dir = "/dev", rules = import_from_fs_rule_library("dev")},

		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},

		{dir = "/sys", rules = import_from_fs_rule_library("sys_hide_selinux")},

		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		-- The real sbox_dir.."/lib/libsb2" must be available:
		--
		-- When libsb2 is installed to target we don't want to map
		-- the path where it is found.  For example gdb needs access
		-- to the library and dynamic linker, and these may be in
		-- target_root, or under sbox_dir.."/lib/libsb2", or
		-- under ~/.scratchbox2.
		{dir = sbox_dir .. "/lib/libsb2",
		 actions = test_first_target_then_host_default_is_target},

		-- -----------------------------------------------
		{prefix = sbox_user_home_dir, use_orig_path = true},

		-- "user" is a special username, and should be mapped
		-- to target_root
		-- (but note that if the real user name is "user",
		-- our previous rule handled that and this rule won't be used)
		{prefix = "/home/user", map_to = target_root,
		 protection = readonly_fs_if_not_root},
		{prefix = "/home/abuild", map_to = target_root},

		-- Other home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},
		-- -----------------------------------------------

		-- The default is to map everything to target_root,
		-- except that we don't map the directory tree where
		-- sb2 was started.
		{prefix = unmapped_workdir, use_orig_path = true},

		{dir = "/usr", rules = emulate_mode_rules_usr},
		{dir = "/bin", rules = emulate_mode_rules_bin},
		{dir = "/etc", rules = emulate_mode_rules_etc},
		{dir = "/var", rules = emulate_mode_rules_var},

		{path = "/", rules = rootdir_rules},
		{prefix = "/", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

-- This allows access to tools with full host paths,
-- this is needed for example to be able to
-- start CPU transparency from tools.
-- Used only when tools_root is set.
local tools_rules = {
		{dir = tools_root, use_orig_path = true},
		{prefix = "/", rules = emulate_mode_rules},
}

if (tools_root ~= nil) and (tools_root ~= "/") then
        -- Tools root is set.
	fs_mapping_rules = tools_rules
else
        -- No tools_root.
	fs_mapping_rules = emulate_mode_rules
end

