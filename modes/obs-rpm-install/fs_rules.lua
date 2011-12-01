-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2011 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "100"
----------------------------------

-- use "==" to test options as long as there is only one possible option,
-- string.match() is slow..
if sbox_mode_specific_options == "use-global-tmp" then
	tmp_dir_dest = "/tmp"
	var_tmp_dir_dest = "/var/tmp"
else
	tmp_dir_dest = session_dir .. "/tmp"
	var_tmp_dir_dest = session_dir .. "/var/tmp"
end

-- If the permission token exists and contains "root", target_root
-- will be available in R/W mode. Otherwise it will be "mounted" R/O.
--local target_root_is_readonly
--if sb.get_session_perm() == "root" then
--	target_root_is_readonly = false
--else
--	target_root_is_readonly = true
--end

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

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
	{ if_exists_then_map_to = tools, protection = readonly_fs_always },
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ map_to = tools, protection = readonly_fs_always }
}

-- Path == "/":
rootdir_rules = {
		-- Special case for /bin/pwd: Some versions don't use getcwd(),
		-- but instead the use open() + fstat() + fchdir() + getdents()
		-- in a loop, and that fails if "/" is mapped to target_root.
		{path = "/", binary_name = "pwd", use_orig_path = true},

		-- All other programs:
		{path = "/", func_name = ".*stat.*",
                    map_to = target_root, protection = readonly_fs_if_not_root },
		{path = "/", func_name = ".*open.*",
                    map_to = target_root, protection = readonly_fs_if_not_root },
		{path = "/", func_name = ".*utime.*",
                    map_to = target_root, protection = readonly_fs_if_not_root },

		-- Default: Map to real root.
		{path = "/", use_orig_path = true},
}


emulate_mode_rules_bin = {
		{path = "/bin/sh", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/bash", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/echo", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/cp", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/rm", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/mv", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/ln", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/ls", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/cat", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/egrep", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/grep", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/mkdir", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/rmdir", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/mktemp", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/chown", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/chmod", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/chgrp", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/gzip", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/bin/sed", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/sort", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/date", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/bin/touch", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		-- rpm rules
		{path = "/bin/rpm",
		 func_name = ".*exec.*",
		 actions = test_first_tools_then_target_default_is_tools},
		-- end of rpm rules
		
		{name = "/bin default rule", dir = "/bin", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr_bin = {
		{path = "/usr/bin/find", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/diff", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/cmp", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/tr", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/dirname", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/basename", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/grep", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/egrep", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/bzip2", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/gzip", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/sed", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/sort", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},
		{path = "/usr/bin/uniq", use_orig_path = true,
		 func_name = ".*exec.*",
		 protection = readonly_fs_always},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, protection = readonly_fs_always},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 protection = readonly_fs_always},

		-- rpm rules
		{prefix = "/usr/bin/rpm",
		 func_name = ".*exec.*",
		 actions = test_first_tools_then_target_default_is_tools},

		-- end of rpm rules
		{name = "/usr/bin default rule", dir = "/usr/bin", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr = {
		{name = "/usr/bin branch", dir = "/usr/bin", rules = emulate_mode_rules_usr_bin},

		-- gdb wants to have access to our dynamic linker also.
		{path = "/usr/lib/libsb2/ld-2.5.so", use_orig_path = true,
		protection = readonly_fs_always},

		{dir = "/usr", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_etc = {
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


emulate_mode_rules = {
		-- First paths that should never be mapped:
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 protection = readonly_fs_always},

		--{prefix = target_root, use_orig_path = true,
		-- protection = readonly_fs_if_not_root},
		{prefix = target_root, use_orig_path = true,
		 -- protection = readonly_fs_if_not_root
		},

		{path = os.getenv("SSH_AUTH_SOCK"), use_orig_path = true},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 protection = readonly_fs_always},

		-- 
		{prefix = "/tmp", replace_by = tmp_dir_dest},

		-- 
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

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

