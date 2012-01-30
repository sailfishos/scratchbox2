-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

argvmods = nil

function load_argvmods_file()
	local argvmods_file_path

	argvmods = {}

	enable_cross_gcc_toolchain = ruletree.catalog_get_boolean(
		"Conf."..active_mapmode, "enable_cross_gcc_toolchain")

	if (enable_cross_gcc_toolchain == true) then
		-- only map gcc & friends if a cross compiler has been defined,
		-- and it has not been disabled by the mapping rules:
		-- (it include the "misc" rules, too)
		argvmods_file_path = session_dir .. "/argvmods_gcc.lua"
	else
		argvmods_file_path = session_dir .. "/argvmods_misc.lua"
	end

	-- load in automatically generated argvmods file
	if sb.path_exists(argvmods_file_path) then
		do_file(argvmods_file_path)
		if debug_messages_enabled then
			sb.log("debug", string.format(
			    "loaded argvmods from '%s'",
			    argvmods_file_path))
		end
	end
end

argvmods_loader_loaded = true
