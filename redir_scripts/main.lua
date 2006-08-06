-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006 Lauri Leukkunen

-- print "hello!\n"

tools_root = os.getenv("SBOX_TOOLS_ROOT")
if (tools_root == nil) then
	tools_root = "/scratchbox/sarge"
end

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


function basename(path)
	if (path == "/") then
		return "/"
	else
		return string.match(path, "[^/]*$")
	end
end

function dirname(path)
	if (path == "/") then
		return "/"
	end
	dir = string.match(path, ".*/")
	if (dir == nil) then
		return "."
	end

	if (dir == "/") then return dir end

	-- chop off the trailing /
	if (string.sub(dir, string.len(dir)) == "/") then
		dir = string.sub(dir, 1, string.len(dir) - 1)
	end
	return dir 
end


-- sbox_translate_path is the function called from libsb2.so
-- preload library and the FUSE system for each path that needs 
-- translating

function sbox_translate_path(binary_name, func_name, work_dir, path)

--	print("debug: [" .. binary_name .. "][" .. func_name .. "][" .. work_dir .. "][" .. path .. "]")

	ret = path
	rp = sb.sb_realpath(path)

	if (rp == "no such file") then
--		print("no such file, path= " .. path )
		if (string.match(func_name, "^exec") or
			string.match(path, "^/bin") or
			string.match(path, "^/usr/bin") or
			string.match(path, "^/usr/local/bin") or
			string.match(path, "^/usr/lib/gcc%-lib/")) then
--			print("no such file and mapping!")
			return tools_root .. "/" .. path
		end
--		print("no such file and not mapping")
		return path
	end

--	print("rp: [" .. rp .. "]")
	dir = dirname(rp)
	file = basename(rp)

--	print("dir: " .. dir)
--	print("file: " .. file)

	-- /scratchbox is special of course
	if (string.match(rp, "^/scratchbox") or
		string.match(rp, "^/home") or
		string.match(rp, "^/tmp") or
		string.match(rp, "^/var") or
		string.match(rp, "^/proc") or
--		string.match(full_path, "^/usr/include") or
--		string.match(file, "*.so$") or
		string.match(dir, "^/$")) then
--		print("not translating..." .. path)
		ret = path
	else
		ret = tools_root .. rp
	end
	return ret	
end

