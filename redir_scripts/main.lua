-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006 Lauri Leukkunen

-- print "hello!\n"


-- SBOX_REDIR_SCRIPTS environment variable controls where
-- we look for the scriptlets defining the path mappings

rsdir = os.getenv("SBOX_REDIR_SCRIPTS")
if (rsdir == nil) then
	rsdir = "/scratchbox/redir_scripts"
end

-- sb.sb_getdirlisting is provided by lua_bindings.c
-- it returns a table listing all files in a directory
t = sb.sb_getdirlisting(rsdir .. "/parts")

-- load the individual parts ($SBOX_REDIR_SCRIPTS/parts/*.lua)
for n = 0,table.maxn(t) do
	if (string.match(t[n], "%a*%.lua$")) then
		-- print("loading part: " .. t[n])
		filename = rsdir .. "/parts/" .. t[n]
		f, err = loadfile(filename)
		if (f == nil) then
			error("\nError while loading " .. filename .. ": \n" .. err .. "\n")
		else
			f() -- execute the loaded chunk
		end
	end
end


-- sbox_translate_path is the function called from libsb2.so
-- preload library and the FUSE system for each path that needs 
-- translating

function sbox_translate_path(binary_name, func_name, orig_path)
	-- print(binary_name .. " " .. func_name .. " " .. orig_path)
		
	if (func_name == "opendir") then
		return orig_path
	end

	return orig_path
end

