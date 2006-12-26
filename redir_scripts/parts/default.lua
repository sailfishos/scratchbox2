-- Copyright (C) 2006 Lauri Leukkunen <lle@rahina.org>
-- Licensed under so called MIT license.

-- print "hello from sample.lua!\n"

-- All these values are treated as Lua patterns, 
-- except the map_to and custom_map_func fields.
-- In map_to these have special meaning:
--
-- "="			map to TARGETDIR .. "/" .. path
-- "=/some/path"	map to TARGETDIR .. "/some/path" .. "/" .. path
-- "+/some/path"	map to COMPILERDIR .. "/some/path"
-- nil			no mapping, use straight
--
-- Any other value is prepended to path (map_to .. "/" .. path).
-- 
-- The rules are exported from this file to the main.lua engine
-- by putting them all into export_rules table variable.
-- They will be evaluated in the order they are listed there.
-- Also the files matching parts/*.lua are sorted alphabetically
-- and used in that order.
--
-- custom_map_func allows you to specify your own path mapping logic.
-- The function takes these parameters: 
-- function(binary_name, func_name, work_dir, real_path, path, rule) 
-- and is expected to return the mapped path. rule argument contains
-- the rule which triggered the function invocation.
-- Any undefined values are equivalent to nil values, except for 
-- binary and func_name, in which case it means ".*"


-- three exec rules for running binaries
default_bin = {
	path = "^/bin",
}

default_usrbin = {
	path = "^/usr/bin",
}

default_usrlocalbin = {
	path = "^/usr/local/bin",
}

default_home = {
	path = "^/home",
}

default_proc = {
	path = "^/proc",
}

default_tmp = {
	path = "^/tmp",
}

default_etc = {
	path = "^/etc",
}

default_scratchbox = {
	path = "^/scratchbox",
}

default_dev = {
	path = "^/dev",
}

libtool = {
	func_name = "exec.*",
	path = ".*libtool",
	map_to = "+/arch_tools/bin"
}

libtoolm4 = {
	path = ".*libtool.m4",
	map_to = "+/arch_tools/share/aclocal"
}

ltdlm4 = {
	path = ".*ltdlm4",
	map_to = "+/arch_tools/share/aclocal"
}

qemu = {
	binary = ".*qemu.*",
	path = "^/",
	map_to = "="
}

autoconf = {
	path = "^/usr/share/autoconf.*"
}

automake = {
	path = "^/usr/share/automake.*"
}

aclocal = {
	path = "^/usr/share/aclocal.*"
}

perl = {
	binary = ".*perl.*",
	path = "^/usr/lib/perl.*"
}

-- catch all rule to map everything else to TARGETDIR/
default_rootdir = {
	path = "^/",
	map_to = "=",
}

export_rules = {
	libtool,
	libtoolm4,
	ltdlm4,
	qemu,
	autoconf,
	automake,
	aclocal,
	perl,
	default_bin,
	default_usrbin,
	default_usrlocalbin,
	default_scratchbox,
	default_dev,
	default_home,
	default_proc,
	default_tmp,
	default_etc,
	default_rootdir
}

