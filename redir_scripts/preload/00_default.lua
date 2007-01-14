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


libtool = {
	func_name = "exec.*",
	path = ".*libtool",
	map_to = "+/arch_tools/bin"
}

libtoolm4 = {
	func_name = ".*",
	path = ".*libtool.m4",
	map_to = "+/arch_tools/share/aclocal"
}

ltdlm4 = {
	func_name = ".*",
	path = ".*ltdlm4",
	map_to = "+/arch_tools/share/aclocal"
}

autoconf = {
	func_name = ".*",
	path = "^/usr/share/autoconf.*",
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


hostgcc = {
	func_name = ".*",
	path = "^/host_usr",
	map_to = "="
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
		libtool,
		libtoolm4,
		ltdlm4,
		autoconf,
		automake,
		aclocal,
		default_bin,
		default_usrbin,
		default_usrlocalbin,
		default_scratchbox,
		default_dev,
		default_home,
		default_proc,
		default_tmp,
		default_etc,
		hostgcc,
		default_rootdir
	}
}

export_chains = { default_chain }

