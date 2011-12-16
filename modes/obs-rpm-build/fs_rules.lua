-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2011 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "101"
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

-- Enable the gcc toolchain tricks.
enable_cross_gcc_toolchain = true

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

-- accelerated programs:
-- Use a binary from tools_root, if it is availabe there.
-- Fallback to target_root, if it doesn't exist in tools.
accelerated_program_actions = {
	{ if_exists_then_map_to = tools, protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always },
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
		{path = "/bin/sh",
		 actions = accelerated_program_actions},
		{path = "/bin/bash",
		 actions = accelerated_program_actions},
		{path = "/bin/echo",
		 actions = accelerated_program_actions},

		{path = "/bin/cp",
		 actions = accelerated_program_actions},
		{path = "/bin/rm",
		 actions = accelerated_program_actions},
		{path = "/bin/mv",
		 actions = accelerated_program_actions},
		{path = "/bin/ln",
		 actions = accelerated_program_actions},
		{path = "/bin/ls",
		 actions = accelerated_program_actions},
		{path = "/bin/cat",
		 actions = accelerated_program_actions},
		{path = "/bin/egrep",
		 actions = accelerated_program_actions},
		{path = "/bin/grep",
		 actions = accelerated_program_actions},

		{path = "/bin/mkdir",
		 actions = accelerated_program_actions},
		{path = "/bin/rmdir",
		 actions = accelerated_program_actions},

		{path = "/bin/mktemp",
		 actions = accelerated_program_actions},

		{path = "/bin/chown",
		 actions = accelerated_program_actions},
		{path = "/bin/chmod",
		 actions = accelerated_program_actions},
		{path = "/bin/chgrp",
		 actions = accelerated_program_actions},

		{path = "/bin/gzip",
		 actions = accelerated_program_actions},

		{path = "/bin/tar",
		 actions = accelerated_program_actions},

		{path = "/bin/sed",
		 actions = accelerated_program_actions},
		{path = "/bin/sort",
		 actions = accelerated_program_actions},
		{path = "/bin/date",
		 actions = accelerated_program_actions},
		{path = "/bin/touch",
		 actions = accelerated_program_actions},

		-- rpm rules
		{path = "/bin/rpm",
		 func_name = ".*exec.*",
		 actions = accelerated_program_actions},
		-- end of rpm rules
		
		{name = "/bin default rule", dir = "/bin", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr_bin = {
		{path = "/usr/bin/awk",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/gawk",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/expr",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/find",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/diff",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/cmp",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/tr",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/dirname",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/basename",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/grep",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/egrep",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/make",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/m4",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/bzip2",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/gzip",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/sed",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/sort",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/uniq",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/bison",
		 actions = accelerated_program_actions},
		{path = "/usr/bin/flex",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/patch",
		 actions = accelerated_program_actions},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, protection = readonly_fs_always},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 protection = readonly_fs_always},

		-- next, automatically generated rules for /usr/bin:
		{name = "/usr/bin autorules", dir = "/usr/bin", rules = argvmods_rules_for_usr_bin,
		 virtual_path = true}, -- don't reverse these.

		-- rpm rules
		{prefix = "/usr/bin/rpm",
		 func_name = ".*exec.*",
		 actions = accelerated_program_actions},

		-- end of rpm rules
		{name = "/usr/bin default rule", dir = "/usr/bin", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr = {
		{name = "/usr/bin branch", dir = "/usr/bin", rules = emulate_mode_rules_usr_bin},

		-- gdb wants to have access to our dynamic linker also.
		{path = "/usr/lib/libsb2/ld-2.5.so", use_orig_path = true,
		protection = readonly_fs_always},

		{dir = "/usr/lib/gcc", actions = test_first_tools_then_target_default_is_tools},

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

emulate_mode_rules_opt = {
		-- for rpmlint:
		{dir = "/opt/testing", 
		 actions = test_first_tools_then_target_default_is_tools},
		--

		{dir = "/opt", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_dev = {
		-- FIXME: This rule should have "protection = eaccess_if_not_owner_or_root",
		-- but that kind of protection is not yet supported.

		-- We can't change times or attributes of host's devices,
		-- but must pretend to be able to do so. Redirect the path
		-- to an existing, dummy location.
		{dir = "/dev", func_name = ".*utime.*",
	         set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

		-- Default: Use real devices.
		{dir = "/dev", use_orig_path = true},
}


emulate_mode_rules = {
		-- First paths that should never be mapped:
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 protection = readonly_fs_always},

		{dir = sbox_target_toolchain_dir, use_orig_path = true,
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

		{dir = "/dev", rules = emulate_mode_rules_dev},

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
		{dir = "/opt", rules = emulate_mode_rules_opt},

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

