-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license

argvmods = {}

-- only map gcc & friends if a cross compiler has been defined
gcc_bindir = os.getenv("SBOX_CROSS_GCC_DIR")
if (gcc_bindir ~= nil and gcc_bindir ~= "") then
	-- this generates gcc related argv/envp manglings
	do_file(lua_scripts .. "/argvenvp_gcc.lua")
end

-- regular mangling rules go here
-- syntax is of the form:
--
-- rule = {
-- 	name = "binary-name",
-- 	add_head = {"list", "of", "args", "to", "prepend"},
-- 	add_tail = {"these", "are", "appended"},
-- 	remove = {"args", "to", "remove"},
-- 	new_filename = "exec-this-binary-instead",
-- 	disable_mapping = 1 -- set this to disable mappings
-- }
-- argvmods[rule.name] = rule
--
-- Environment modifications are not supported yet, except for disabling
-- mappings.
--
-- TODO:
--
-- * new_filename should probably be replaced by integrating argv/envp
--   mangling with the path mapping machinery.

dpkg_architecture = {
	name = "dpkg-architecture",
	remove = {"-f"}
}
argvmods[dpkg_architecture.name] = dpkg_architecture


-- returns: err, file, argc, argv, envc, envp
-- (zero as "err" means "OK")
function sbox_execve_mod(filename, argv, envp)
	local new_argv = {}
	local new_envp = {}
	local binaryname = string.match(filename, "[^/]+$")
	local new_filename = filename

	-- print(string.format("sbox_execve_mod(): %s\n", filename))
	
	new_envp = envp

	am = argvmods[binaryname]
	if (am and not am.remove) then am.remove = {} end
	if (am and not am.add_head) then am.add_head = {} end
	if (am and not am.add_tail) then am.add_tail = {} end

	if (am ~= nil) then
		-- head additions
		for i = 1, table.maxn(am.add_head) do
			table.insert(new_argv, am.add_head[i])
		end
	
		-- populate new_argv, skip those that are to be removed
		for i = 1, table.maxn(argv) do
			local match = 0
			for j = 1, table.maxn(am.remove) do
				if (argv[i] == am.remove[j]) then
					match = 1
				end
			end
			if (match == 0) then
				table.insert(new_argv, argv[i])
			end
		end
		
		-- tail additions
		for i = 1, table.maxn(am.add_tail) do
			table.insert(new_argv, am.add_tail[i])
		end

		if (am.new_filename) then
			-- print(string.format("changing to: %s\n", am.new_filename))
			new_filename = am.new_filename
			new_argv[1] = am.new_filename
		end
		if (am.disable_mapping) then
			table.insert(new_envp, "SBOX_DISABLE_MAPPING=1")
			table.insert(new_envp, "SBOX_DISABLE_ARGVENVP=1")
		end
	else
		new_argv = argv
	end
	return 0, new_filename, #new_argv, new_argv, #new_envp, new_envp
end
