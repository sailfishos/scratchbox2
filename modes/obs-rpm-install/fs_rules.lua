-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2011 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "105"
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

-- package management programs:
-- Cannot enable acceleration for RPM related programs if it seems that some of
-- the PACKAGES were updated in the tools root.
function can_accelerate_rpm()
    local test = [=[
    tools=%q
    target=%q

    PACKAGES=(rpm libsolv0 zypper)

    # OBS build stores preinstalled packages here
    PREINSTALL_CACHE=$target/.init_b_cache/rpms

    query_version()
    {
        # It is crucial to not write single byte on stderr - it would be treated as a sign of
        # failure, causing sb2 to exit with error "errors detected during sb2d startup."
        rpm -q --queryformat "%%{VERSION}" "$@" 2>/dev/null
    }

    for package in "${PACKAGES[@]}"; do
        if ! ver_tools=$(query_version --root "$tools" "$package"); then
            # The package does not seem to be installed in tools root. Enabling acceleration
            # cannot break anything in this case.
            continue
        fi

        if ! ver_target=$(query_version --root "$target" "$package") \
            && ! ver_target=$(query_version -p "$PREINSTALL_CACHE/$package.rpm"); then
            # Cannot determine version of the package in build target. Maybe it is not
            # installed (yet), maybe the mechanisms we tried need update. Not enabling acceleration.
            exit 1
        fi

        if [[ $ver_tools != $ver_target ]]; then
            exit 1
        fi
    done

    exit 0
    ]=]

	local rc = os.execute(string.format(test, tools, target_root))
	return rc == 0
end

if can_accelerate_rpm() then
	rpm_program_actions = accelerated_program_actions
else
	rpm_program_actions = {
		{ map_to = target_root, protection = readonly_fs_always },
	}
end

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


emulate_mode_rules_bin = {
		{path = "/bin/sh",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/bash",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/echo",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/cp",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/rm",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/mv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/ln",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/ls",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/cat",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/egrep",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/grep",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/mkdir",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/rmdir",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/mktemp",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/chown",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/chmod",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/chgrp",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/gzip",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/bin/sed",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/sort",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/date",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/bin/touch",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		-- rpm rules
		{path = "/bin/rpm",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		-- end of rpm rules
		
		{name = "/bin default rule", dir = "/bin", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr_bin = {
		{path = "/usr/bin/find",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/diff",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/cmp",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/tr",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/dirname",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/basename",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/grep",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/egrep",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/bzip2",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/gzip",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/sed",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/sort",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/uniq",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, protection = readonly_fs_always},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 protection = readonly_fs_always},

		-- rpm rules
		{path = "/usr/bin/zypper",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/deltainfoxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/dumpsolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/installcheck",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/mergesolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/repomdxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/rpmdb2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/rpmmd2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/rpms2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/testsolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{path = "/usr/bin/updateinfoxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},
		{prefix = "/usr/bin/rpm",
		 func_class = FUNC_CLASS_EXEC,
		 actions = rpm_program_actions},

		-- end of rpm rules
		{name = "/usr/bin default rule", dir = "/usr/bin", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr = {
		{name = "/usr/bin branch", dir = "/usr/bin", rules = emulate_mode_rules_usr_bin},

		-- gdb wants to have access to our dynamic linker also,
		-- /usr/lib/libsb2/wrappers/*, etc.
		{dir = "/usr/lib/libsb2", use_orig_path = true,
		 protection = readonly_fs_always},

                {path = "/usr/lib/rpm/elfdeps", func_class = FUNC_CLASS_EXEC,
		 actions=rpm_program_actions},
                {path = "/usr/lib/rpm/debugedit", func_class = FUNC_CLASS_EXEC,
		 actions=rpm_program_actions},
                {path = "/usr/lib/rpm/javadeps", func_class = FUNC_CLASS_EXEC,
		 actions=rpm_program_actions},
                {path = "/usr/lib/rpm/rpmdeps", func_class = FUNC_CLASS_EXEC,
		 actions=rpm_program_actions},

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
		{dir = "/var/run", map_to = session_dir},
		{dir = "/var/tmp", replace_by = var_tmp_dir_dest},

		{dir = "/var", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_home = {
		-- We can't change times or attributes of the real /home
		-- but must pretend to be able to do so. Redirect the path
		-- to an existing, dummy location.
		{path = "/home",
		 func_class = FUNC_CLASS_SET_TIMES,
	         set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

		-- Default: Not mapped, R/W access.
		{dir = "/home", use_orig_path = true},
}

emulate_mode_rules_opt = {
		{dir = "/opt", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

sys_rules = {
		{path = "/sys",
		 func_class = FUNC_CLASS_SET_TIMES,
	         set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },
		{dir = "/sys", use_orig_path = true},
}


emulate_mode_rules = {
		-- First paths that should never be mapped:
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 protection = readonly_fs_always},

		--{dir = target_root, use_orig_path = true,
		-- protection = readonly_fs_if_not_root},
		{dir = target_root, use_orig_path = true,
		 virtual_path = true, -- don't try to reverse this
		 -- protection = readonly_fs_if_not_root
		},

		{path = os.getenv("SSH_AUTH_SOCK"), use_orig_path = true},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{dir = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 protection = readonly_fs_always},

		-- Many RPMs invoke ldconfig using absolute path in their %post* scripts
		{path = "/sbin/ldconfig",
		 func_class = FUNC_CLASS_EXEC + FUNC_CLASS_OPEN,
		 replace_by = session_dir .. "/wrappers." .. active_mapmode .. "/ldconfig",
		 protection = readonly_fs_always},

		-- 
		{dir = "/tmp", replace_by = tmp_dir_dest},

		{dir = "/dev", rules = import_from_fs_rule_library("dev")},

		{dir = "/proc", rules = import_from_fs_rule_library("proc")},
		{dir = "/sys", rules = sys_rules},

		{dir = sbox_dir .. "/share/scratchbox2",
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
		-- home directories:
		{dir = "/home", rules = emulate_mode_rules_home},
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

-- allow user to extend these rules with a ~/.sbrules file
import_from_fs_rule_library("user_rules")

-- Define /parentroot as being outside like /home, this is a Mer SDK
-- path convention
use_outside_path("/parentroot")

-- Now run ~/.sbrules
run_user_sbrules()

if (tools_root ~= nil) and (tools_root ~= "/") then
        -- Tools root is set.
	fs_mapping_rules = tools_rules
else
        -- No tools_root.
	fs_mapping_rules = emulate_mode_rules
end

