-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

tools = tools_root
if (not tools) then
	tools = "/"
end

sb2_home_dir = os.getenv("HOME") .. "/.scratchbox2/"
sb2_share_dir = sb2_home_dir .. os.getenv("SBOX_TARGET") .. "/share"

sb2_session_dir = os.getenv("SBOX_SESSION_DIR")
if (not sb2_session_dir) then
	sb2_session_dir = "/tmp"
end

-- =========== Actions for conditional rules ===========

test_first_target_then_tools_default_is_target = {
	{ if_exists_then_map_to = target_root },
	{ if_exists_then_map_to = tools, readonly = true },
	{ map_to = target_root }
}

test_first_tools_default_is_target = {
	{ if_exists_then_map_to = tools, readonly = true },
	{ map_to = target_root }
}

-- =========== Mapping rule chains ===========

simple_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- -----------------------------------------------
		-- 1. General SB2 & maemo environment:

		{prefix = "/usr/bin/sb2-",
		 use_orig_path = true, readonly = true},
		{prefix = "/opt/maemo",
		 use_orig_path = true, readonly = true},
		{prefix = "/usr/share/scratchbox2/host_usr",
		 replace_by = "/usr", readonly = true},
		{prefix = "/usr/share/scratchbox2",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 2. Home directories

		-- "user" is a special username on the Maemo platform:
		{prefix = "/home/user", map_to = target_root},

		-- Other users = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},

		-- -----------------------------------------------
		-- 20. /bin/* and /usr/bin/*:
		-- tools that need special processing:

		{path = "/bin/sh",
		 replace_by = tools .. "/bin/bash", readonly = true},
		{prefix = "/usr/bin/host-",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 30. /lib/*

		{prefix = "/lib", map_to = target_root},

		-- -----------------------------------------------
		-- 40. /usr/lib/*
		-- Most of /usr/lib should come from target_root, but
		-- there are exceptions: Some tools have private subdirectories
		-- there.

		{prefix = "/usr/lib/perl", map_to = tools, readonly = true},
		{prefix = "/usr/lib/dpkg", map_to = tools, readonly = true},
		{prefix = "/usr/lib/apt", map_to = tools, readonly = true},
		{prefix = "/usr/lib/cdbs", map_to = tools, readonly = true},
		{prefix = "/usr/lib/libfakeroot", map_to = tools, readonly = true},

		-- /usr/lib/python* from tools_root
		{prefix = "/usr/lib/python", map_to = tools, readonly = true},

		{prefix = "/usr/lib", map_to = target_root},

		-- -----------------------------------------------
		-- 45. /usr/share/aclocal*
		-- This is more than a bit complex, we must mix files from
		-- both places:
		-- Prefer files in tools_root, but if not there, try
		-- to get it from target_root. New files will be created
		-- to target_root.
		-- directory itself comes from target_root; there are several
		-- lib*-dev packages that add files to the aclocal directories
		-- (because of this, opendir() must open the diretory from
		-- target_root)
		--
		-- First, existing directories => target_root:
		{path = "/usr/share/aclocal",
		 actions = test_first_target_then_tools_default_is_target},
		{path = "/usr/share/aclocal-1.4",
		 actions = test_first_target_then_tools_default_is_target},
		{path = "/usr/share/aclocal-1.7",
		 actions = test_first_target_then_tools_default_is_target},
		{path = "/usr/share/aclocal-1.8",
		 actions = test_first_target_then_tools_default_is_target},
		{path = "/usr/share/aclocal-1.9",
		 actions = test_first_target_then_tools_default_is_target},
		{path = "/usr/share/aclocal-1.10",
		 actions = test_first_target_then_tools_default_is_target},

		-- Next, exceptions to these rules:
		-- 1) gnome-common presents policy problems, typically we
		--    have it in both places but want to take it from the
		--    rootstrap:
		{prefix = "/usr/share/aclocal/gnome-common",
		 actions = test_first_target_then_tools_default_is_target},
		{prefix = "/usr/share/aclocal/gnome-compiler",
		 actions = test_first_target_then_tools_default_is_target},

		-- Next, use /usr/share/aclocal* from tools_root if target
		-- exists, but default is target_root
		{prefix = "/usr/share/aclocal",
		 actions = test_first_tools_default_is_target},

		-- -----------------------------------------------
		-- 46. /usr/share/* (other than /usr/share/aclocal*)
		--
		-- The default is to map /usr/share to tools_root,
		-- but there are lots of exceptions. That directory
		-- is used for so many purposes nowadays..

		-- (see the comment about gnome-common files in .../aclocal):
		{prefix = "/usr/share/gnome-common",
		 actions = test_first_target_then_tools_default_is_target},

		{prefix = "/usr/share/glib-2.0", map_to = target_root},
		{prefix = "/usr/share/dbus-1", map_to = target_root},

		-- zoneinfo belongs to "libc6" package:
		{prefix = "/usr/share/zoneinfo", map_to = target_root},

		-- files from package 'microb-engine-dev':
		{path = "/usr/share/idl", map_to = target_root},
		{prefix = "/usr/share/idl/microb-engine", map_to = target_root},

		-- /usr/share/hildon* (this is a real prefix):
		-- (was added to map hildon-theme-layout-4)
		{prefix = "/usr/share/hildon", map_to = target_root},

		-- for libwww0:
		{prefix = "/usr/share/w3c-libwww", map_to = target_root},

		-- for libwww-dev:
		{prefix = "/usr/share/libwww", map_to = target_root},

		-- for libsofia-sip-ua-dev:
		{prefix = "/usr/share/sofia-sip", map_to = target_root},

		{prefix = "/usr/share/osso", map_to = target_root},
		{prefix = "/usr/share/doc", map_to = target_root},

		-- for liblzo-dev:
		{prefix = "/usr/share/lzo", map_to = target_root},

		-- for modest-providers-data:
		{prefix = "/usr/share/modest", map_to = target_root},

		-- default rules:
		{path = "/usr/share", map_to = tools, readonly = true},
		{prefix = "/usr/share/", map_to = tools, readonly = true},

		-- -----------------------------------------------
		-- 50. /usr/src/*
		{path = "/usr/src", map_to = target_root},
		{prefix = "/usr/src/", map_to = target_root},

		-- -----------------------------------------------
		-- 55. X11 (/usr/X11R6/*)

		{prefix = "/usr/X11R6/lib", map_to = target_root},
		{prefix = "/usr/X11R6/include", map_to = target_root},

		-- -----------------------------------------------
		-- 60. /usr/include/*

		{prefix = "/usr/include", map_to = target_root},

		-- -----------------------------------------------
		-- 70. /etc/*
		--
		{prefix = "/etc/gconf/2", map_to = target_root},
		{prefix = "/etc/dbus-1", map_to = target_root},
		{prefix = "/etc/osso-af-init", map_to = target_root},
		{prefix = "/etc/gtk-2.0", map_to = target_root},

		{prefix = "/etc/apt", map_to = target_root},

		-- Files that must not be mapped:
		{prefix = "/etc/resolv.conf",
		 use_orig_path = true, readonly = true},
		{path = "/etc/passwd",
		 use_orig_path = true, readonly = true},
		{path = "/etc/shadow",
		 use_orig_path = true, readonly = true},

		-- default rules:
		{path = "/etc", map_to = tools, readonly = true},
		{prefix = "/etc/", map_to = tools, readonly = true},

		-- -----------------------------------------------
		-- 80. /var/*

		-- files from package "xkbutils":
		{prefix = "/var/lib/xkb", map_to = target_root},

		-- apt & dpkg:
		{prefix = "/var/lib/apt", map_to = target_root},
		{prefix = "/var/cache/apt", map_to = target_root},
		{prefix = "/var/lib/dpkg", map_to = target_root},
		{prefix = "/var/cache/dpkg", map_to = target_root},
		{prefix = "/var/cache/debconf", map_to = target_root},

		{prefix = "/var/lib/dbus", map_to = target_root},
		{prefix = "/var/run", map_to = target_root},

		{prefix = "/var/log", map_to = target_root},

		-- -----------------------------------------------
		-- 85. /tmp
		{prefix = sb2_session_dir, use_orig_path = true},
		{prefix = "/tmp", map_to = sb2_session_dir},

		-- -----------------------------------------------
		-- 90. Top-level directories that must not be mapped:
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- 95. Some virtual paths:
		{prefix = "/host_usr", map_to = target_root},

		-- -----------------------------------------------
		-- 98. Scratchbox 1 emulation rules
		-- (some packages have hard-coded paths to the SB1 enviroment;
		-- replace those by the correct locations in our environment)

		-- "libtool" for arm
		{prefix = "/scratchbox/compilers/cs2005q3.2-glibc2.5-arm/arch_tools/share/libtool",
		 replace_by = sb2_share_dir .. "/libtool",
		 log_level = "warning",
		 readonly = true},

		-- "libtool" for i386
		{prefix = "/scratchbox/compilers/cs2005q3.2-glibc-i386/arch_tools/share",
		 replace_by = tools .. "/usr/share",
		 log_level = "warning",
		 readonly = true},

		{prefix = "/scratchbox/tools/bin",
		 replace_by = tools .. "/usr/bin",
		 log_level = "warning",
		 readonly = true},

		{prefix = "/scratchbox/tools/autotools/automake-1.7/share/automake-1.7",
		 replace_by = tools .. "/usr/share/automake-1.7",
		 log_level = "warning",
		 readonly = true},

		-- otherwise, don't map /scratchbox, some people still
		-- keep their projects there.
		{prefix = "/scratchbox", use_orig_path = true},

		-- -----------------------------------------------
		-- 100. DEFAULT RULES:
		-- the root directory must not be mapped:
		{path = "/", use_orig_path = true},

		-- ..but everything else defaults to tools_root,
		-- except that tools_root should not be mapped twice.
		{prefix = tools, use_orig_path = true},
		{prefix = "/", map_to = tools, readonly = true}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = basename(os.getenv("SBOX_CPUTRANSPARENCY_METHOD")),
	rules = {
		{prefix = "/lib", map_to = target_root},
		{prefix = "/usr/lib", map_to = target_root},
		{prefix = "/usr/local/lib", map_to = target_root},

		{prefix = sb2_session_dir, use_orig_path = true},
		{prefix = "/tmp", map_to = sb2_session_dir},

		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},

		{prefix = "/etc/resolv.conf",
		 use_orig_path = true, readonly = true},
		{path = "/etc/passwd",
		 use_orig_path = true, readonly = true},
		{path = "/etc/shadow",
		 use_orig_path = true, readonly = true},

		{prefix = tools, use_orig_path = true},

		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = tools}
	}
}


export_chains = {
	qemu_chain,
	simple_chain
}
