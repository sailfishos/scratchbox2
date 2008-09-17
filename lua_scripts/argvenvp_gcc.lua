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

-- The trick with ":" .. is to have a non-prefixed gcc call caught here
for prefix in string.gmatch(":" .. sbox_cross_gcc_prefix_list, "[^:]*") do
	for i = 1, table.maxn(gcc_compilers) do
		tmp = {}
		tmp.name = prefix .. gcc_compilers[i]
		tmp.new_filename = sbox_cross_gcc_dir .. "/" .. sbox_cross_gcc_subst_prefix .. gcc_compilers[i]
		tmp.add_tail = {}
		tmp.remove = {}
		if (sbox_cross_gcc_specs_file and sbox_cross_gcc_specs_file ~= "") then
			table.insert(tmp.add_tail, "-specs="..sbox_cross_gcc_specs_file)
		end
		if (sbox_extra_cross_compiler_args and sbox_extra_cross_compiler_args ~= "") then
			for gcc_extra in string.gmatch(sbox_extra_cross_compiler_args, "[^ ]+") do
				table.insert(tmp.add_tail, gcc_extra)
			end
		end
		if (sbox_extra_cross_compiler_stdinc and sbox_extra_cross_compiler_stdinc ~= "") then
			for gcc_stdinc in string.gmatch(sbox_extra_cross_compiler_stdinc, "[^ ]+") do
				table.insert(tmp.add_tail, gcc_stdinc)
			end
		end
		if (sbox_block_cross_compiler_args and sbox_block_cross_compiler_args ~= "") then
			for gcc_block in string.gmatch(sbox_block_cross_compiler_args, "[^ ]+") do
				table.insert(tmp.remove, gcc_block)
			end
		end
		argvmods[tmp.name] = tmp
	end
	
	-- just map the filename for linkers and tools
	for i = 1, table.maxn(gcc_linkers) do
		tmp = {}
		tmp.name = prefix .. gcc_linkers[i]
		tmp.new_filename = sbox_cross_gcc_dir .. "/" .. sbox_cross_gcc_subst_prefix .. gcc_linkers[i]
		tmp.add_tail = {}
		tmp.remove = {}
		if (sbox_extra_cross_ld_args and sbox_extra_cross_ld_args ~= "") then
			for ld_extra in string.gmatch(sbox_extra_cross_ld_args, "[^ ]+") do
				table.insert(tmp.add_tail, ld_extra)
			end
		end
		if (sbox_block_cross_ld_args and sbox_block_cross_ld_args ~= "") then
			for ld_block in string.gmatch(sbox_block_cross_ld_args, "[^ ]+") do
				table.insert(tmp.remove, ld_block)
			end
		end
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = sbox_cross_gcc_dir .. "/" .. sbox_cross_gcc_subst_prefix .. gcc_tools[i]
		argvmods[tmp.name] = tmp
	end
end


-- deal with host-gcc functionality, disables mapping
for prefix in string.gmatch(sbox_host_gcc_prefix_list, "[^:]+") do
	for i = 1, table.maxn(gcc_compilers) do
		tmp = {}
		tmp.name = prefix .. gcc_compilers[i]
		tmp.new_filename = sbox_host_gcc_dir .. "/" .. sbox_host_gcc_subst_prefix .. gcc_compilers[i]
		tmp.add_tail = {}
		tmp.remove = {}
		tmp.disable_mapping = 1
		if (sbox_extra_host_compiler_args and sbox_extra_host_compiler_args ~= "") then
			for gcc_extra in string.gmatch(sbox_extra_host_compiler_args, "[^ ]+") do
				table.insert(tmp.add_tail, gcc_extra)
			end
		end
		if (sbox_block_host_compiler_args and sbox_block_host_compiler_args ~= "") then
			for gcc_block in string.gmatch(sbox_block_host_compiler_args, "[^ ]+") do
				table.insert(tmp.remove, gcc_block)
			end
		end
		argvmods[tmp.name] = tmp
	end

	-- just map the filename for linkers and tools
	for i = 1, table.maxn(gcc_linkers) do
		tmp = {}
		tmp.name = prefix .. gcc_linkers[i]
		tmp.new_filename = sbox_host_gcc_dir .. "/" .. sbox_host_gcc_subst_prefix .. gcc_linkers[i]
		tmp.disable_mapping = 1
		argvmods[tmp.name] = tmp
	end
	for i = 1, table.maxn(gcc_tools) do
		tmp = {}
		tmp.name = prefix .. gcc_tools[i]
		tmp.new_filename = sbox_host_gcc_dir .. "/" .. sbox_host_gcc_subst_prefix .. gcc_tools[i]
		tmp.disable_mapping = 1
		argvmods[tmp.name] = tmp
	end
end

-- end of gcc related generation


