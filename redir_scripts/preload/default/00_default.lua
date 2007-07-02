-- Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under so called MIT license.

default_bin = {
	func_name = ".*",
	path = "^/bin",
	map_to = nil
}

default_usrbin = {
	func_name = ".*",
	path = "^/usr/bin",
	map_to = nil
}

default_usrlocalbin = {
	func_name = ".*",
	path = "^/usr/local/bin",
	map_to = nil
}

default_sbin = {
	func_name = ".*",
	path = "^/sbin",
	map_to = nil
}

default_usrsbin = {
	func_name = ".*",
	path = "^/usr/sbin",
	map_to = nil
}

default_home = {
	func_name = ".*",
	path = "^/home",
	map_to = nil
}

default_proc = {
	func_name = ".*",
	path = "^/proc",
	map_to = nil
}

default_tmp = {
	func_name = ".*",
	path = "^/tmp",
	map_to = nil
}

default_etc = {
	func_name = ".*",
	path = "^/etc",
	map_to = nil
}

default_scratchbox = {
	func_name = ".*",
	path = "^/scratchbox",
	map_to = nil
}

default_dev = {
	func_name = ".*",
	path = "^/dev",
	map_to = nil
}

default_opt = {
	func_name = ".*",
	path = "^/opt",
	map_to = nil
}

autoconf = {
	func_name = ".*",
	path = "^/usr/share/autoconf.*",
	map_to = nil
}

autoconf_misc = {
	func_name = ".*",
	path = "^/usr/share/misc/config.*",
	map_to = nil
}

automake = {
	func_name = ".*",
	path = "^/usr/share/automake.*",
	map_to = nil
}

aclocal = {
	func_name = ".*",
	path = "^/usr/share/aclocal.*",
	map_to = nil
}

intltool = {
	func_name = ".*",
	path = "^/usr/share/intltool.*",
	map_to = nil
}

debhelper = {
	func_name = ".*",
	path = "^/usr/share/debhelper.*",
	map_to = nil
}

hostgcc = {
	func_name = ".*",
	path = "^/host_usr",
	map_to = "="
}


-- pkgconfig

pkgconfig = {
	func_name = ".*",
	path = "^/usr/lib/pkgconfig.*",
	map_to = "="
}

-- don't map anything else from TARGETDIR

targetdir = {
	func_name = ".*",
	path = "^" .. target_root .. ".*",
	map_to = nil
}

-- don't map "/" path
actual_root = {
	func_name = ".*",
	path = "^/$",
	map_to = nil
}

-- catch all rule to map everything else to TARGETDIR/
default_rootdir = {
	func_name = ".*",
	path = "^/",
	map_to = "="
}


-- the actual chain, this is not actually exported
-- it's only defined in this file which gets loaded
-- first by main.lua so that default_chain is available
-- for the actual entry chains defined in the other
-- lua files
default_chain = {
	next_chain = nil,
	noentry = 1, -- never use this chain directly to start mapping
	binary = nil,
	rules = {
		autoconf,
		autoconf_misc,
		automake,
		aclocal,
		intltool,
		debhelper,
		default_bin,
		default_usrbin,
		default_usrlocalbin,
		default_sbin,
		default_usrsbin,
		default_scratchbox,
		default_dev,
		default_opt,
		default_home,
		default_proc,
		default_tmp,
		default_etc,
		hostgcc,
		pkgconfig,
		targetdir,
		actual_root,
		default_rootdir
	}
}

export_chains = { default_chain }

