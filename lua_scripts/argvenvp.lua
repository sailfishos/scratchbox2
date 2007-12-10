-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license


gcc_compilers = {
"cc",
"gcc",
"c++",
"g++",
"cpp",
"f77",
"g77"
}

gcc_linkers = {
"ld"
}

gcc_tools = {
"addr2line",
"ar",
"as",
"c++filt",
"gccbug",
"gcov",
"nm",
"objcopy",
"objdump",
"ranlib",
"readelf",
"size",
"strings",
"strip"
}

argvmods = {}

gcc_bindir = os.getenv("SBOX_CROSS_GCC_DIR")
gcc_subst_prefix = os.getenv("SBOX_CROSS_GCC_SUBST_PREFIX")
gcc_extra_args = os.getenv("SBOX_EXTRA_CROSS_COMPILER_ARGS")
gcc_block_args = os.getenv("SBOX_BLOCK_CROSS_COMPILER_ARGS")
host_gcc_bindir = os.getenv("SBOX_HOST_GCC_DIR")
host_gcc_subst_prefix = os.getenv("SBOX_HOST_GCC_SUBST_PREFIX")
host_gcc_extra_args = os.getenv("SBOX_EXTRA_HOST_COMPILER_ARGS")
host_gcc_block_args = os.getenv("SBOX_BLOCK_HOST_COMPILER_ARGS")

-- The trick with ":" .. is to have a non-prefixed gcc call caught here
for prefix in string.gmatch(":" .. os.getenv("SBOX_CROSS_GCC_PREFIX_LIST"), "[^:]*") do
	for i = 1, table.maxn(gcc_compilers) do
		tmp = {}
		tmp.name = prefix .. gcc_compilers[i]
		tmp.new_filename = gcc_bindir .. "/" .. gcc_subst_prefix .. gcc_compilers[i]
		tmp.add = {}
		tmp.remove = {}
		if (gcc_extra_args) then
			for gcc_extra in string.gmatch(gcc_extra_args, "[^ ]+") do
				table.insert(tmp.add, gcc_extra)
			end
		end
		if (gcc_block_args) then
			for gcc_block in string.gmatch(gcc_block_args, "[^ ]+") do
				table.insert(tmp.remove, gcc_block)
			end
		end
		argvmods[tmp.name] = tmp
	end
	
	-- just map the filename for linkers and tools
	for i = 1, table.maxn(gcc_linkers) do
		tmp = {}
		tmp.name = prefix .. gcc_linkers[i]
		tmp.new_filename = gcc_bindir .. "/" .. gcc_subst_prefix .. gcc_linkers[i]
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = gcc_bindir .. "/" .. gcc_subst_prefix .. gcc_tools[i]
		argvmods[tmp.name] = tmp
	end
end

for prefix in string.gmatch(os.getenv("SBOX_HOST_GCC_PREFIX_LIST"), "[^:]+") do
	for i = 1, table.maxn(gcc_compilers) do
		tmp = {}
		tmp.name = prefix .. gcc_compilers[i]
		tmp.new_filename = host_gcc_bindir .. "/" .. host_gcc_subst_prefix .. gcc_compilers[i]
		tmp.add = {}
		tmp.remove = {}
		if (host_gcc_extra_args) then
			for gcc_extra in string.gmatch(host_gcc_extra_args, "[^ ]+") do
				table.insert(tmp.add, gcc_extra)
			end
		end
		if (host_gcc_block_args) then
			for gcc_block in string.gmatch(host_gcc_block_args, "[^ ]+") do
				table.insert(tmp.remove, gcc_block)
			end
		end
		argvmods[tmp.name] = tmp
	end

	-- just map the filename for linkers and tools
	for i = 1, table.maxn(gcc_linkers) do
		tmp = {}
		tmp.name = prefix .. gcc_linkers[i]
		tmp.new_filename = host_gcc_bindir .. "/" .. host_gcc_subst_prefix .. gcc_linkers[i]
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = host_gcc_bindir .. "/" .. host_gcc_subst_prefix .. gcc_tools[i]
		argvmods[tmp.name] = tmp
	end
end

dpkg_architecture = {
	name = "dpkg-architecture",
	remove = {"-f"},
	add = {}
}

argvmods[dpkg_architecture.name] = dpkg_architecture


function sbox_execve_mod(filename, argv, envp)
	local new_argv = {}
	local new_envp = {}
	local binaryname = string.match(filename, "[^/]+$")
	local new_filename = filename

	am = argvmods[binaryname]

	if (am ~= nil) then
		-- check removals
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
		-- additions
		for i = 1, table.maxn(am.add) do
			table.insert(new_argv, am.add[i])
		end
		if (am.new_filename) then
			new_filename = am.new_filename
		end
	else
		new_argv = argv
	end
	new_envp = envp
	return 0, new_filename, #new_argv, new_argv, #new_envp, new_envp
end
