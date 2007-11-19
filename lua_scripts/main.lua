-- Scratchbox2 Lua main file
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.


function do_file(filename)
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" 
			.. err .. "\n")
	else
		f() -- execute the loaded chunk
	end
end

lua_scripts = os.getenv("SBOX_LUA_SCRIPTS")

do_file(lua_scripts .. "/mapping.lua")
do_file(lua_scripts .. "/argvenvp.lua")

