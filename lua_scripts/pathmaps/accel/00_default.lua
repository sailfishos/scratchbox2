-- Author: Lauri T. Aarnio
-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (c) 2009 Nokia Corporation.
-- Licensed under MIT license.
--
-- "accel" mode = build accelerator mode, to be used for software 
-- development & building when the rootstrap and the tools are "twins":
-- Built from the same sources, but tools contains native binaries while
-- the rootstrap contains target binaries.


-- Rule file interface version, mandatory.
--
rule_file_interface_version = "25"
----------------------------------

tools = tools_root
if (not tools) then
	tools = "/"
end

sb2_share_dir = sbox_user_home_dir.."/.scratchbox2/"..sbox_target.."/share"

-- =========== Exec policies:  ===========

-- If the permission token exists and contains "root",
-- use fakeroot
local fakeroot_ld_preload = ""
if sb.get_session_perm() == "root" then
	target_root_is_readonly = false
	fakeroot_ld_preload = ":"..host_ld_preload_fakeroot
else
	target_root_is_readonly = true
end

--
-- For tools: If tools_root is set and libsb2 has been installed there,
-- then dynamic libraries can be used from tools_root (otherwise we'll
-- be using the libs from the host OS)

devel_mode_tools_ld_so = nil		-- default = not needed
devel_mode_tools_ld_library_path_prefix = ""
devel_mode_tools_ld_library_path_suffix = ""
-- localization support for tools
devel_mode_locale_path = nil
devel_mode_gconv_path = nil

if ((tools_root ~= nil) and conf_tools_sb2_installed) then
	if (conf_tools_ld_so ~= nil) then
		-- Ok to use dynamic libraries from tools!
		devel_mode_tools_ld_so = conf_tools_ld_so
	end
	devel_mode_tools_ld_library_path_prefix = conf_tools_ld_so_library_path
	if (conf_tools_locale_path ~= nil) then
		-- use locales from tools
		devel_mode_locale_path = conf_tools_locale_path
		devel_mode_gconv_path = conf_tools_gconv_path
	end
else
	devel_mode_tools_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	devel_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

tools_script_interp_rules = {
	rules = {
		{dir = "/scratchbox/tools/bin",
		 replace_by = tools .. "/usr/bin", log_level = "warning"},

		{prefix = "/", map_to = tools}
	}
}

exec_policy_tools = {
	name = "Tools",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

exec_policy_tools_perl = {
	name = "Tools-perl",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (t.p)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

exec_policy_tools_python = {
	name = "Tools-python",
	native_app_ld_so = devel_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = devel_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,

	native_app_locale_path = devel_mode_locale_path,
	native_app_gconv_path = devel_mode_gconv_path,

	script_log_level = "debug",
	script_log_message = "SCRIPT from tools (t.p)",
	script_interpreter_rules = tools_script_interp_rules,
	script_set_argv0_to_mapped_interpreter = true,
}

-- For target binaries:
-- First, note that "foreign" binaries are easy to handle, no problem there.
-- But if CPU transparency method has not been set, then host CPU == target CPU:
-- we have "target's native" and "host's native" binaries, that would look 
-- identical (and valid!) to the kernel. But they need to use different 
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

devel_mode_target_ld_so = nil		-- default = not needed
devel_mode_target_ld_library_path_prefix = ""
devel_mode_target_ld_library_path_suffix = nil

if (conf_target_sb2_installed) then
	if (conf_target_ld_so ~= nil) then
		-- use dynamic libraries from target, 
		-- when executing native binaries!
		devel_mode_target_ld_so = conf_target_ld_so

		-- FIXME: This exec policy should process (map components of)
		-- the current value of LD_LIBRARY_PATH, and add the results
		-- to devel_mode_target_ld_library_path just before exec.
		-- This has not been done yet.
	end
	devel_mode_target_ld_library_path_prefix = conf_target_ld_so_library_path
else
	devel_mode_target_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	devel_mode_target_ld_library_path_suffix =
		host_ld_library_path_suffix
end

exec_policy_target = {
	name = "Rootstrap",
	native_app_ld_so = devel_mode_target_ld_so,
	native_app_ld_so_supports_argv0 = conf_target_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_target_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = target_root,
	native_app_ld_so_supports_nodefaultdirs = conf_target_ld_so_supports_nodefaultdirs,

	native_app_locale_path = conf_target_locale_path,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,

	native_app_ld_library_path_prefix = devel_mode_target_ld_library_path_prefix,
	native_app_ld_library_path_suffix = devel_mode_target_ld_library_path_suffix,
}

--
-- For real host binaries:

exec_policy_host_os = {
	name = "Host",
	log_level = "debug",
	log_message = "executing in host OS mode",

	script_interpreter_rules = {
		rules = {
			{prefix = "/", use_orig_path = true}
		}
	},

	-- native_app_ld_library_path* can be left undefined,
	-- host settings will be used (but then it won't add user's
	-- LD_LIBRARY_PATH, which is exactly what we want).
	-- native_app_ld_preload* is not set because of the same reason.
}

-- =========== Actions for conditional rules ===========

test_first_target_then_tools_default_is_target = {
	{ if_exists_then_map_to = target_root, readonly = true },
	{ if_exists_then_map_to = tools, readonly = true },
	{ map_to = target_root, readonly = true }
}

test_first_tools_then_target_default_is_tools = {
	{ if_exists_then_map_to = tools, readonly = true },
	{ if_exists_then_map_to = target_root, readonly = true },
	{ map_to = tools, readonly = true }
}

test_first_tools_default_is_target = {
	{ if_exists_then_map_to = tools, readonly = true },
	{ map_to = target_root, readonly = true }
}

test_first_usr_bin_default_is_bin__replace = {
	{ if_exists_then_replace_by = tools.."/usr/bin", readonly = true },
	{ if_exists_then_replace_by = target_root.."/usr/bin", readonly = true },
	{ if_exists_then_replace_by = tools.."/bin", readonly = true },
	{ replace_by = target_root.."/bin", readonly = true }
}

test_first_tools_default_is_orig_path = {
	{ if_exists_then_map_to = tools, readonly = true },
	{ use_orig_path = true, readonly = true},
}

test_usr_share_aclocal__replace = {
	{ if_exists_then_replace_by = target_root.."/usr/share/aclocal", readonly = true },
	{ if_exists_then_replace_by = tools.."/usr/share/aclocal", readonly = true },
	{ replace_by = target_root.."/usr/share/aclocal", readonly = true }
}

perl_lib_test = {
	{ if_active_exec_policy_is = "Tools-perl",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

-- /usr/bin/perl, /usr/bin/python and related tools should
-- be mapped to target_root, but there are two exceptions:
-- 1) perl and python scripts from tools_root need to get
--    the interpreter from tools_root, too
-- 2) if the program can not be found from target_root,
--    but is present in tools_root, it will be used from
--    tools. This produces a warnign, because it isn't always
--    safe to do so.
perl_bin_test = {
	{ if_redirect_ignore_is_active = "/usr/bin/perl",
	  map_to = target_root, readonly = true },
	{ if_redirect_force_is_active = "/usr/bin/perl",
	  map_to = tools, readonly = true,
	  exec_policy = exec_policy_tools_perl },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Tools-perl",
	  map_to = tools, readonly = true },
	{ if_exists_then_map_to = target_root, readonly = true },
	{ if_exists_then_map_to = tools,
	  log_level = "warning", log_message = "Mapped to tools_root",
	  readonly = true },
	{ map_to = target_root, readonly = true }
}

python_bin_test = {
	{ if_redirect_ignore_is_active = "/usr/bin/python",
	  map_to = target_root, readonly = true },
	{ if_redirect_force_is_active = "/usr/bin/python",
	  map_to = tools, readonly = true,
	  exec_policy = exec_policy_tools_python },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Tools-python",
	  map_to = tools, readonly = true },
	{ if_exists_then_map_to = target_root, readonly = true },
	{ if_exists_then_map_to = tools,
	  log_level = "warning", log_message = "Mapped to tools_root",
	  readonly = true },
	{ map_to = target_root, readonly = true }
}

python_lib_test = {
	{ if_active_exec_policy_is = "Tools-python",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

terminfo_test = {
	{ if_active_exec_policy_is = "Tools",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

--
-- Message catalogs (LC_MESSAGES) are taken based on
-- active exec policy.
--
message_catalog_test = {
	{ if_active_exec_policy_is = "Tools",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Rootstrap",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

-- =========== Mapping rule chains ===========

-- "/bin/*"
devel_mode_rules_bin = {
	rules = {
		-- -----------------------------------------------
		-- 20. /bin/*:
		-- tools that need special processing:

		{path = "/bin/sh",
		 replace_by = tools .. "/bin/bash", readonly = true},

		{path = "/bin/ps",
		 use_orig_path = true, readonly = true},
	
		-- "pwd" from "coreutils" uses a built-in replacement
		-- for getcwd(), which does not work with sb2, because
		-- the path mapping means that ".." does not necessarily
		-- contain an i-node that refers to the current directory..
		{path = "/bin/pwd",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode .. "/pwd",
		 readonly = true},

		{dir = "/bin",
		 actions = test_first_tools_default_is_target},
	}
}

-- Used when dir = "/usr/share":
devel_mode_rules_usr_share = {
	rules = {
		-- -----------------------------------------------
		-- 1. General SB2 environment:

		{prefix = sbox_dir .. "/share/scratchbox2/host_usr",
		 replace_by = "/usr", readonly = true},
		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- Perl:
		{prefix = "/usr/share/perl", actions = perl_lib_test},

		-- Python:
		{prefix = "/usr/share/python", actions = python_lib_test},
		{prefix = "/usr/share/pygobject", actions = python_lib_test},

		-- -----------------------------------------------

		{dir = "/usr/share/aclocal", map_to = target_root,
		 readonly = true},

		-- 100. DEFAULT RULES:
		{prefix = "/usr/share",
		 actions = test_first_target_then_tools_default_is_target},
	}
}

-- Used when dir = "/usr/bin":
devel_mode_rules_usr_bin = {
	rules = {
		-- -----------------------------------------------
		-- 1. General SB2 environment:

		{prefix = "/usr/bin/sb2-",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 20. /usr/bin/*:
		-- tools that need special processing:

		{prefix = "/usr/bin/host-",
		 actions = test_first_tools_default_is_orig_path},

		-- "localedef" *must* be used from the target, the version
		-- which exists in tools_root appers to work but doesn't..
		{path = "/usr/bin/localedef", map_to = target_root,
		 readonly = true},

		-- "chrpath" comes from target, too, but we haven't 
		-- verified what happens if it was used from tools.
		{path = "/usr/bin/chrpath", map_to = target_root,
		 readonly = true},

		-- 19. perl & python:
		-- 	processing depends on SBOX_REDIRECT_IGNORE,
		--	SBOX_REDIRECT_FORCE and 
		--	name of the current exec policy. 
		--	(these are real prefixes, version number may
		--	be included in the name (/usr/bin/python2.5 etc))
		{prefix = "/usr/bin/perl", actions = perl_bin_test},
		{prefix = "/usr/bin/python", actions = python_bin_test},

		-- next, automatically generated rules for /usr/bin:
		{dir = "/usr/bin", chain = argvmods_rules_for_usr_bin,
		 virtual_path = true}, -- don't reverse these.

		-- and finally, the default:
		{dir = "/usr/bin",
		 actions = test_first_tools_default_is_target},
	}
}


-- Used when dir = "/usr":
devel_mode_rules_usr = {
	rules = {
		{dir = "/usr/share", chain = devel_mode_rules_usr_share},

		{dir = "/usr/bin", chain = devel_mode_rules_usr_bin},

		-- -----------------------------------------------
		-- 40. /usr/lib/*
		-- Most of /usr/lib should come from target_root, but
		-- there are exceptions: Some tools have private subdirectories
		-- there.

		{dir = "/usr/lib/gcc", actions = test_first_tools_then_target_default_is_tools},

		{prefix = "/usr/lib/perl", actions = perl_lib_test},

		{prefix = "/usr/lib/python", actions = python_lib_test},

		{prefix = "/usr/lib/dpkg", map_to = tools, readonly = true},
		{prefix = "/usr/lib/apt", map_to = tools, readonly = true},

		{prefix = "/usr/lib/libfakeroot", map_to = tools, readonly = true},

		{prefix = "/usr/lib",
		 actions = test_first_target_then_tools_default_is_target},

		-- -----------------------------------------------
		-- 50. /usr/src/*
		{path = "/usr/src", map_to = target_root, readonly = true},
		{prefix = "/usr/src/", map_to = target_root, readonly = true},

		-- -----------------------------------------------
		-- 55. X11 (/usr/X11R6/*)

		{prefix = "/usr/X11R6/lib", map_to = target_root,
		 readonly = true},
		{prefix = "/usr/X11R6/include", map_to = target_root,
		 readonly = true},

		-- -----------------------------------------------
		-- 60. /usr/include/*

		{prefix = "/usr/include", map_to = target_root,
		 readonly = true},

		-- -----------------------------------------------
		{dir = "/usr/sbin",
		 actions = test_first_tools_default_is_target},

		-- -----------------------------------------------
		-- 100. DEFAULT for /usr/*:
		{prefix = "/usr",
		 actions = test_first_tools_default_is_target},
	}
}

devel_mode_rules_etc = {
	rules = {
		-- -----------------------------------------------
		-- 70. /etc/*
		--
		{prefix = "/etc/gconf/2", map_to = target_root,
		 readonly = true},
		{prefix = "/etc/dbus-1", map_to = target_root,
		 readonly = true},
		{prefix = "/etc/osso-af-init", map_to = target_root,
		 readonly = true},
		{prefix = "/etc/gtk-2.0", map_to = target_root,
		 readonly = true},

		{prefix = "/etc/apt", map_to = target_root, readonly = true},
		{prefix = "/etc/terminfo", actions = terminfo_test},

		-- Perl & Python:
		{prefix = "/etc/perl", actions = perl_lib_test},
		{prefix = "/etc/python", actions = python_lib_test},

		-- Files that must not be mapped:
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolv.conf",
		 force_orig_path = true, readonly = true},
		{path = "/etc/passwd",
		 use_orig_path = true, readonly = true},
		{path = "/etc/shadow",
		 use_orig_path = true, readonly = true},

		-- Symlinks in /etc/alternatives are used to select tools:
		{dir = "/etc/alternatives",
		 actions = test_first_tools_default_is_target},

		-- default rules:
		{dir = "/etc",
		 actions = test_first_target_then_tools_default_is_target},
	}
}

devel_mode_rules_var = {
	rules = {
		-- -----------------------------------------------
		-- 80. /var/*

		{prefix = "/var/run/resolvconf", force_orig_path = true,
		 readonly = true},

		{prefix = "/var/run", map_to = session_dir},

		-- files from package "xkbutils":
		{prefix = "/var/lib/xkb", map_to = target_root,
		 readonly = true},

		-- apt & dpkg:
		{prefix = "/var/lib/apt", map_to = target_root,
		 readonly = true},
		{prefix = "/var/cache/apt", map_to = target_root,
		 readonly = true},
		{prefix = "/var/lib/dpkg", map_to = target_root,
		 readonly = true},
		{prefix = "/var/cache/dpkg", map_to = target_root,
		 readonly = true},
		{prefix = "/var/cache/debconf", map_to = target_root,
		 readonly = true},

		{prefix = "/var/lib/dbus", map_to = target_root,
		 readonly = true},

		{prefix = "/var/log", map_to = target_root,
		 readonly = true},

		{dir = "/var/tmp", map_to = session_dir},

		-- default rules:
		{dir = "/var",
		 actions = test_first_target_then_tools_default_is_target},
	}
}

-- Path == "/":
devel_mode_rules_scratchbox1 = {
	rules = {
		-- -----------------------------------------------
		-- 98. Scratchbox 1 emulation rules
		-- (some packages have hard-coded paths to the SB1 enviroment;
		-- replace those by the correct locations in our environment)
		-- (these are marked "virtual"; these won't be reversed)
		-- "libtool" for arm
		{prefix = "/scratchbox/compilers/cs2005q3.2-glibc2.5-arm/arch_tools/share/libtool",
		 replace_by = sb2_share_dir .. "/libtool",
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- "libtool" for i386
		{prefix = "/scratchbox/compilers/cs2005q3.2-glibc-i386/arch_tools/share",
		 replace_by = tools .. "/usr/share",
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- compiler tools:
		{prefix = "/scratchbox/compilers/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- Scratchbox 1 tools/bin
		--
		-- set exec_policy for perl & python:
		{prefix = "/scratchbox/tools/bin/perl",
		 replace_by = tools .. "/usr/bin/perl",
	  	 exec_policy = exec_policy_tools_perl,
		 log_level = "warning",
		 readonly = true, virtual_path = true},
		{prefix = "/scratchbox/tools/bin/python",
		 replace_by = tools .. "/usr/bin/python",
	  	 exec_policy = exec_policy_tools_python,
		 log_level = "warning",
		 readonly = true, virtual_path = true},
		--
		-- Other tools:
		{prefix = "/scratchbox/tools/bin",
		 actions = test_first_usr_bin_default_is_bin__replace,
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- "policy-rc.d" checks if scratchbox-version exists, 
		-- to detect if it is running inside scratchbox..
		{prefix = "/scratchbox/etc/scratchbox-version",
		 replace_by = "/usr/share/scratchbox2/version",
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		{prefix = "/scratchbox/tools/autotools/automake-1.7/share/automake-1.7",
		 replace_by = tools .. "/usr/share/automake-1.7",
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- otherwise, don't map /scratchbox, some people still
		-- keep their projects there.
		{prefix = "/scratchbox", use_orig_path = true},
	}
}

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
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 readonly = true},

		{prefix = "/sb2/scripts",
		 replace_by = sbox_dir.."/share/scratchbox2/scripts",
		 readonly = true},

		-- tools_root should not be mapped twice.
		{prefix = tools, use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 5. Maemo SDK+

		{prefix = "/opt/maemo",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 10. Home directories

		{prefix = sbox_user_home_dir, use_orig_path = true},

		-- "user" is a special username at least on the Maemo platform:
		-- (but note that if the real user name is "user",
		-- our previous rule handled that and this rule won't be used)
		{prefix = "/home/user", map_to = target_root},

		-- Home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},

		-- -----------------------------------------------
		-- 30. /lib/*

		-- 
		-- terminfo search path is by default:
		-- /etc/terminfo, /lib/terminfo and /usr/share/terminfo
		-- we map these depending on active exec policy.
		--
		-- Other rules are in /etc and /usr/share rules above.
		--
		{prefix = "/lib/terminfo", actions = terminfo_test},

		{prefix = "/lib", map_to = target_root, readonly = true},

		-- -----------------------------------------------
		-- 40. /usr
		{dir = "/usr", chain = devel_mode_rules_usr},

		-- -----------------------------------------------
		-- 70. /etc/*
		--
		{dir = "/etc", chain = devel_mode_rules_etc},

		-- -----------------------------------------------
		-- 80. /var/*
		{dir = "/var", chain = devel_mode_rules_var},

		-- -----------------------------------------------
		-- 85. /tmp
		{dir = "/tmp", map_to = session_dir},

		-- -----------------------------------------------
		-- 90. Top-level directories that must not be mapped:
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 95. Some virtual paths:
		{prefix = "/host_usr", map_to = target_root, readonly = true},

		-- unmodified view of the rootstrap, can be used as destination
		-- directory when installing stuff to the rootstrap
		-- This gives R/W access to the target_root (with -R)
		{prefix = "/target_root", replace_by = target_root,
		 readonly = target_root_is_readonly},

		-- -----------------------------------------------
		-- 98. Scratchbox 1 emulation rules
		-- (some packages have hard-coded paths to the SB1 enviroment;
		-- replace those by the correct locations in our environment)
		-- (these are marked "virtual"; these won't be reversed)
		{dir = "/scratchbox", chain = devel_mode_rules_scratchbox1},

		-- -----------------------------------------------
		-- 100. DEFAULT RULES:
		-- the root directory must not be mapped:
		{path = "/", use_orig_path = true},

		-- "standard" directories default to tools_root
		-- (except that bin and sbin also test for target-
		-- specific programs from target_root),
		-- but everything else defaults to the host system
		-- (so that things like /mnt, /media and /opt are
		-- used from the host)
		{dir = "/bin", chain = devel_mode_rules_bin},

		{dir = "/sbin",
		 actions = test_first_tools_default_is_target},

		-- Default = Host, R/W access
		{prefix = "/", use_orig_path = true}
	}
}

-- Path mapping rules.
export_chains = {
	simple_chain
}

-- Exec policy rules.
-- These are used only if the mapping rule did not provide an exec policy.
--
-- Note that the real path (mapped path) is used
--  when looking up rules from here!
devel_exec_policies = {
	next_chain = nil,
	binary = nil,
	rules = {

		-- Tools:
		-- (tools must be listed first, the tools directory
		-- might be under user's home directory)
		{prefix = tools .. "/usr/bin/perl",
		 exec_policy = exec_policy_tools_perl},
		{prefix = tools .. "/usr/bin/python",
		 exec_policy = exec_policy_tools_python},
		{prefix = tools, exec_policy = exec_policy_tools},

		-- ~/bin probably contains programs for the host OS:
                {prefix = sbox_user_home_dir.."/bin",
		 exec_policy = exec_policy_host_os},

                -- Other places under the home directory are expected
                -- to contain target binaries:
                {prefix = sbox_user_home_dir, exec_policy = exec_policy_target},

		-- Target binaries:
		{prefix = target_root, exec_policy = exec_policy_target},

		-- the place where the session was created is expected
		-- to contain target binaries:
		{prefix = sbox_workdir, exec_policy = exec_policy_target},

		-- -----------------------------------------------
		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy = exec_policy_host_os}
	}
}

exec_policy_chains = {
	devel_exec_policies
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_host_os,
	exec_policy_target,
	exec_policy_tools,
	exec_policy_tools_perl,
	exec_policy_tools_python,
}

