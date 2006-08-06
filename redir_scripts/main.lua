-- Scratchbox2 universal redirector dynamic path translation scripts
-- Copyright (C) 2006 Lauri Leukkunen

-- print "hello!\n"

tools_root = "/scratchbox/sarge"

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
	return string.match(path, "[^/]*$")
end

function dirname(path)
	dir = string.match(path, ".*/")

	-- root ('/') needs to be special
	if (string.len(dir) == 1) then 
		return dir
	end
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
	if (string.sub(path, 1, 1) == "/") then
		-- print("absolute path")
		dir = dirname(path)
	else
		-- print("relative path")
		dir = dirname(work_dir .. path)
	end

	file = basename(path)
	if (string.len(dir) > 1) then
		full_path = dir .. "/" .. file
	else
		full_path = dir .. file
	end

--	print("full_path: " .. full_path)
--	print("dir: " .. dir)
--	print("file: " .. file)

	-- /scratchbox is special of course
	if (string.match(full_path, "^/scratchbox") or
		string.match(full_path, "^/home") or
		string.match(full_path, "^/tmp") or
		string.match(full_path, "^/var") or
		string.match(full_path, "^/proc") or
--		string.match(full_path, "^/usr/include") or
--		string.match(file, "*.so$") or
		string.match(dir, "^/$")) then
--		print("not translating...")
		ret = path
	else
		ret = tools_root .. full_path
	end

	tmp = sb.sb_followsymlink(ret)
--	print(ret .. " -> " .. tmp)
	if (string.find(tmp, ret, 1, true) == 1) then
--		print("complete: " .. tmp)
		return ret
	else
		if (string.find(tmp, "/") ~= 1) then
			-- relative symlink, track from dir
			tmp = dir .. "/" .. tmp
		end
--		print("recurse! " .. tmp)
		-- tail recurse until no more symlinks
		return sbox_translate_path(binary_name, func_name, work_dir, tmp)
	end
end

