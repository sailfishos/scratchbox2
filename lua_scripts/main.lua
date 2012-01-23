-- Scratchbox2 Lua main file
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

-- This file is loaded by the libsb2.so preload library, from the 
-- constructor to initialize sb2's "Lua-side"

debug = os.getenv("SBOX_MAPPING_DEBUG")
debug_messages_enabled = sb.debug_messages_enabled()
exec_engine_loaded = false

-- This version string is used to check that the lua scripts offer 
-- what the C files expect, and v.v.
-- Increment the number whenever the interface beween Lua and C is changed.
--
-- NOTE: the corresponding identifier for C is in include/sb2.h,
-- see that file for description about differences
sb2_lua_c_interface_version = "95"

function do_file(filename)
	if (debug_messages_enabled) then
		sb.log("debug", string.format("Loading '%s'", filename))
	end
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" 
			.. err .. "\n")
		-- "error()" never returns
	else
		f() -- execute the loaded chunk
	end
end

session_dir = sb.get_session_dir()

-- Load session-specific settings
do_file(session_dir .. "/sb2-session.conf")

-- Load path mapping functions
--
-- NOTE: "mapping.lua" loads the mapping mode config, which may be needed
-- by "argvenvp.lua", so order is important!
do_file(session_dir .. "/lua_scripts/mapping.lua")

-- other processes than "make" or the shells load
-- argvenvp.lua only if exec* functions are needed!

function sbox_execve_preprocess_loader(binaryname, argv, envp)
	local prev_fn = sbox_execve_preprocess

	sb.log("info", "sbox_execve_preprocess called: loading argvenvp.lua")
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")

	if prev_fn == sbox_execve_preprocess then
		sb.log("error",
			"Fatal: Failed to load real sbox_execve_preprocess")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return sbox_execve_preprocess(binaryname, argv, envp)
end

function sb_execve_postprocess_loader(rule, exec_policy, exec_type,
		mapped_file, filename, binaryname, argv, envp)
	local prev_fn = sb_execve_postprocess

	sb.log("info", "sb_execve_postprocess called: loading argvenvp.lua")
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")

	if prev_fn == sb_execve_postprocess then
		sb.log("error",
			"Fatal: Failed to load real sb_execve_postprocess")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return sb_execve_postprocess(rule, exec_policy, exec_type,
		mapped_file, filename, binaryname, argv, envp)
end

function sbox_get_host_policy_ld_params_loader()
	local prev_fn = sbox_get_host_policy_ld_params

	sb.log("info", "sbox_get_host_policy_ld_params called: loading argvenvp.lua")
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")

	if prev_fn == sbox_get_host_policy_ld_params then
		sb.log("error",
			"Fatal: Failed to load real sbox_get_host_policy_ld_params")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return sbox_get_host_policy_ld_params()
end

function sbox_map_network_addr(realfnname, protocol, addr_type,
	orig_dst_addr, orig_port, binary_name)

	local prev_fn = sbox_map_network_addr

	sb.log("info", "sbox_map_network_addr called: loading network.lua")
	do_file(session_dir .. "/lua_scripts/network.lua")

	if prev_fn == sbox_map_network_addr then
		sb.log("error",
			"Fatal: Failed to load real sbox_map_network_addr")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return sbox_map_network_addr(realfnname, protocol,
		addr_type, orig_dst_addr, orig_port, binary_name)
end

local binary_name = sb.get_binary_name()

if (binary_name == "make") or
   (binary_name == "sh") or
   (binary_name == "bash") or
   (binary_name == "find") then
	-- This is a performance optimization;
	-- this process will probably do multiple fork()+exec*() calls,
	-- so it is better to load the exec code right now.
	-- otherwise every child process will be doing the loading..that
	-- would work, of course, but this is better when the overall
	-- performance is considered.
	if debug_messages_enabled then
		sb.log("debug", "Loading exec code now")
	end
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")
else
	if debug_messages_enabled then
		sb.log("debug", "exec code will be loaded on demand")
	end
	sbox_execve_preprocess = sbox_execve_preprocess_loader
	sb_execve_postprocess = sb_execve_postprocess_loader
	sbox_get_host_policy_ld_params = sbox_get_host_policy_ld_params_loader
end

-- sb2 is ready for operation!
