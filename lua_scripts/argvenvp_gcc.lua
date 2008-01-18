-- Copyright (C) 2008 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license

-- Here is the necessary plumbing to generate gcc related
-- argv/envp manglings

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
ld_extra_args = os.getenv("SBOX_EXTRA_CROSS_LD_ARGS")
ld_block_args = os.getenv("SBOX_BLOCK_CROSS_LD_ARGS")
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
		tmp.add_tail = {}
		tmp.remove = {}
		if (gcc_extra_args) then
			for gcc_extra in string.gmatch(gcc_extra_args, "[^ ]+") do
				table.insert(tmp.add_tail, gcc_extra)
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
		tmp.add_tail = {}
		tmp.remove = {}
		if (ld_extra_args) then
			for ld_extra in string.gmatch(ld_extra_args, "[^ ]+") do
				table.insert(tmp.add_tail, ld_extra)
			end
		end
		if (ld_block_args) then
			for ld_block in string.gmatch(ld_block_args, "[^ ]+") do
				table.insert(tmp.remove, ld_block)
			end
		end
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = gcc_bindir .. "/" .. gcc_subst_prefix .. gcc_tools[i]
		argvmods[tmp.name] = tmp
	end
end


-- deal with host-gcc functionality, disables mapping
for prefix in string.gmatch(os.getenv("SBOX_HOST_GCC_PREFIX_LIST"), "[^:]+") do
	for i = 1, table.maxn(gcc_compilers) do
		tmp = {}
		tmp.name = prefix .. gcc_compilers[i]
		tmp.new_filename = host_gcc_bindir .. "/" .. host_gcc_subst_prefix .. gcc_compilers[i]
		tmp.add_tail = {}
		tmp.remove = {}
		tmp.disable_mapping = 1
		if (host_gcc_extra_args) then
			for gcc_extra in string.gmatch(host_gcc_extra_args, "[^ ]+") do
				table.insert(tmp.add_tail, gcc_extra)
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
		tmp.disable_mapping = 1
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = host_gcc_bindir .. "/" .. host_gcc_subst_prefix .. gcc_tools[i]
		tmp.disable_mapping = 1
		argvmods[tmp.name] = tmp
	end
end

-- end of gcc related generation


