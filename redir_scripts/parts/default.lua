-- Copyright (C) 2006 Lauri Leukkunen <lle@rahina.org>
-- Licensed under so called MIT license.

-- print "hello from sample.lua!\n"

-- All these values are treated as Lua patterns, 
-- except the map_to and custom_map_func fields.
-- In map_to these have special meaning:
--
-- "="			map to TARGETDIR .. "/" .. path
-- "=/some/path"	map to TARGETDIR .. "/some/path" .. "/" .. path
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


-- three exec rules for running binaries
default_bin = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/bin",
	map_to = nil,
	custom_map_func = nil
}

default_usrbin = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/usr/bin",
	map_to = nil,
	custom_map_func = nil
}

default_usrlocalbin = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/usr/local/bin",
	map_to = nil,
	custom_map_func = nil
}

default_home = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/home",
	map_to = nil,
	custom_map_func = nil
}

default_proc = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/proc",
	map_to = nil,
	custom_map_func = nil
}

default_tmp = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/tmp",
	map_to = nil,
	custom_map_func = nil
}

default_etc = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/etc",
	map_to = nil,
	custom_map_func = nil
}

default_scratchbox = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/scratchbox",
	map_to = nil,
	custom_map_func = nil
}

-- catch all rule to map everything else to TARGETDIR/
default_rootdir = {
	binary = ".*",
	func_name = ".*",
	func_param = nil,
	path = "^/",
	map_to = "=",
	custom_map_func = nil
}

export_rules = {
	default_bin,
	default_usrbin,
	default_usrlocalbin,
	default_scratchbox,
	default_home,
	default_proc,
	default_tmp,
	default_etc,
	default_rootdir
}

