-- Copyright (C) 2008 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license

-- Here is the necessary plumbing to generate gcc related
-- argv/envp manglings
--
-- See syntax from argvenvp.lua.
--

gcc_compilers = {
	"cc",
	"gcc",
	"c++",
	"g++",
	"cpp",
	"f77",
	"g77"
}

-- names, where cross_gcc_shortversion may be embedded to the name
-- (e.g. gcc-3.4, g++-3.4)
gcc_compilers_with_version = {
	"gcc",
	"g++",
	"cpp"
}

gcc_linkers = {
	"ld",
	"ld.bfd",
	"ld.gold"
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

local generic_gcc_tools_path_prefixes = {
	"/usr/bin/",
	"/sb2/"
}

function register_gcc_component_path(tmp, gccrule)
	-- Path prefixes:
	-- 1. currently all cross-gcc tools that we are going to replace
	--    live in /usr/bin, but these tools may call other tools from
	--    the same set (e.g. "gcc" calls "ld", etc). That is why
	--    cross_gcc_dir is needed, too.
	-- 2. note that cross_gcc_dir is not empty, this file
	--    won't be loaded at all if it is (see argvenvp.lua),
	-- 3. Wrappers for host-* tools live in /sb2/wrappers.
	local gcc_tools_path_prefixes = {}

    -- lua array copy wtf
	for j,x in ipairs(generic_gcc_tools_path_prefixes) do
		table.insert(gcc_tools_path_prefixes, x)
	end

	if gccrule == nil then
		return
	end

	if gccrule.cross_gcc_dir ~= nil then
		table.insert(gcc_tools_path_prefixes, gccrule.cross_gcc_dir)
	end

	if gccrule.cross_gcc_progs_path ~= nil then
		for path in string.gmatch(gccrule.cross_gcc_progs_path,"[^:]+") do
			table.insert(gcc_tools_path_prefixes, path)
		end
	end

	tmp.path_prefixes = gcc_tools_path_prefixes
	argvmods[tmp.name] = tmp
end

function gcc_compiler_arg_mods(tmp, gccrule)
	tmp.add_tail = {}
	tmp.remove = {}
	if (gccrule.cross_gcc_specs_file and gccrule.cross_gcc_specs_file ~= "") then
		table.insert(tmp.add_tail, "-specs="..gccrule.cross_gcc_specs_file)
	end
	if (gccrule.extra_cross_compiler_args and gccrule.extra_cross_compiler_args ~= "") then
		for gcc_extra in string.gmatch(gccrule.extra_cross_compiler_args, "[^ ]+") do
			table.insert(tmp.add_tail, gcc_extra)
		end
	end
	if (gccrule.extra_cross_compiler_stdinc and gccrule.extra_cross_compiler_stdinc ~= "") then
		for gcc_stdinc in string.gmatch(gccrule.extra_cross_compiler_stdinc, "[^ ]+") do
			table.insert(tmp.add_tail, gcc_stdinc)
		end
	end
	if (gccrule.block_cross_compiler_args and gccrule.block_cross_compiler_args ~= "") then
		for gcc_block in string.gmatch(gccrule.block_cross_compiler_args, "[^ ]+") do
			table.insert(tmp.remove, gcc_block)
		end
	end
end

function add_cross_compiler(gccrule, version)
	local require_version = true

	if version == "" then
		require_version = false
	end
	
	-- The trick with ":" .. is to have a non-prefixed gcc call caught here
	for prefix in string.gmatch(":" .. gccrule.cross_gcc_prefix_list, "[^:]*") do

		if require_version == false then
			-- Compiler tools without version suffix
			for i = 1, table.maxn(gcc_compilers) do
				local tmp = {}
				tmp.name = prefix .. gcc_compilers[i]
				tmp.new_filename = gccrule.cross_gcc_dir .. "/" .. gccrule.cross_gcc_subst_prefix .. gcc_compilers[i]
				gcc_compiler_arg_mods(tmp, gccrule)
				register_gcc_component_path(tmp, gccrule)
			end
		end

		-- Compiler tools with version suffix
		for i = 1, table.maxn(gcc_compilers_with_version) do
			local tmp = {}
			tmp.name = prefix .. gcc_compilers_with_version[i] .. "-" ..
				gccrule.cross_gcc_shortversion
			tmp.new_filename = gccrule.cross_gcc_dir .. "/" .. gccrule.cross_gcc_subst_prefix .. gcc_compilers_with_version[i]
			gcc_compiler_arg_mods(tmp, gccrule)
			register_gcc_component_path(tmp, gccrule)
		end
		
		if require_version == false then
			-- just map the filename for linkers and tools
			for i = 1, table.maxn(gcc_linkers) do
				local tmp = {}
				tmp.name = prefix .. gcc_linkers[i]
				tmp.new_filename = gccrule.cross_gcc_dir .. "/" .. gccrule.cross_gcc_subst_prefix .. gcc_linkers[i]
				tmp.add_tail = {}
				tmp.remove = {}
				if (gccrule.extra_cross_ld_args and gccrule.extra_cross_ld_args ~= "") then
					for ld_extra in string.gmatch(gccrule.extra_cross_ld_args, "[^ ]+") do
						table.insert(tmp.add_tail, ld_extra)
					end
				end
				if (gccrule.block_cross_ld_args and gccrule.block_cross_ld_args ~= "") then
					for ld_block in string.gmatch(gccrule.block_cross_ld_args, "[^ ]+") do
						table.insert(tmp.remove, ld_block)
					end
				end
				register_gcc_component_path(tmp, gccrule)
			end
			for i = 1, table.maxn(gcc_tools) do
				local tmp = {}
				tmp.name = prefix .. gcc_tools[i]
				tmp.new_filename = gccrule.cross_gcc_dir .. "/" .. gccrule.cross_gcc_subst_prefix .. gcc_tools[i]
				register_gcc_component_path(tmp, gccrule)
			end
		end
	end
end

if (sbox_host_gcc_prefix_list and sbox_host_gcc_prefix_list ~= "") then
	-- deal with host-gcc functionality, disables mapping
	for prefix in string.gmatch(sbox_host_gcc_prefix_list, "[^:]+") do
		for i = 1, table.maxn(gcc_compilers) do
			local tmp = {}
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
			register_gcc_component_path(tmp, nil)
		end

		-- just map the filename for linkers and tools
		for i = 1, table.maxn(gcc_linkers) do
			local tmp = {}
			tmp.name = prefix .. gcc_linkers[i]
			tmp.new_filename = sbox_host_gcc_dir .. "/" .. sbox_host_gcc_subst_prefix .. gcc_linkers[i]
			tmp.disable_mapping = 1
			register_gcc_component_path(tmp, nil)
		end
		for i = 1, table.maxn(gcc_tools) do
			local tmp = {}
			tmp.name = prefix .. gcc_tools[i]
			tmp.new_filename = sbox_host_gcc_dir .. "/" .. sbox_host_gcc_subst_prefix .. gcc_tools[i]
			tmp.disable_mapping = 1
			register_gcc_component_path(tmp, nil)
		end
	end
end

gcc_rule_file_path = session_dir .. "/gcc-conf.lua"

if (sblib.path_exists(gcc_rule_file_path)) then
	sblib.log("debug", "Loading GCC rules")
	do_file(gcc_rule_file_path)
end

-- end of gcc related generation


