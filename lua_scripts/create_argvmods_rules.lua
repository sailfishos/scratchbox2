--
-- Copyright (c) 2009 Nokia Corporation.
--
-- Licensed under MIT licence
--
-- This script reads in argvmods_xxx.lua file (xxx being here
-- gcc or misc) and writes out lua table containing generated
-- argvmods rules.
--
argvmods = {}

if not exec_engine_loaded then
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")
end

local allowed_rulenames = {
	"name",
	"path_prefixes",
	"add_head",
	"add_options",
	"add_tail",
	"remove",
	"new_filename",
	"disable_mapping",
	"drivers",
}

local src_files_basenames=os.getenv("SBOX_ARGVMODS_SOURCE_FILES")

print("--")
print("-- Generator: create_argvmods_rules.lua")
print("-- Source files: " .. src_files_basenames)
print("-- Automatically generated rules.  Do not modify:")
print("--")

local count = 0

function stringlist_to_string(tbl)
	local s = ""
	for i = 1, #tbl do
		assert(type(tbl[i]) == "string")

		s = s..string.format("\t\"%s\",\n", tbl[i])
	end
	return s
end

argvmods = {}
for src_basename in string.gmatch(src_files_basenames, "[^:]+") do

	local argvmods_source_file = session_dir .. "/lua_scripts/" ..
	    src_basename..".lua"

	print("-- src="..argvmods_source_file)
	
	do_file(argvmods_source_file)
end

-- Check rulenames.
for binary_name, rule in pairs(argvmods) do
	for name, value in pairs(rule) do
		--
		-- Check that rulename is valid.
		--
		local found = 0
		for i = 1, #allowed_rulenames do
			if name == allowed_rulenames[i] then
				found = 1
				break
			end
		end
		if found == 0 then
			io.stderr:write(string.format(
			    "invalid rulename '%s'\n", name))
		end
	end
end

-- A contents => variable name lookup table
table2tablename = {}
table2tablename_n = 0

-- Convert string table elements.
for binary_name, rule in pairs(argvmods) do
	for name, value in pairs(rule) do
		if type(value) == "table" then
			if #value ~= 0 then
				local x = stringlist_to_string(value)
				local z = name.."\n"..x
				if table2tablename[z] == nil then
					local z_name = string.format("z_argvmods_%s_%d",
						name, table2tablename_n)
					table2tablename[z] = z_name
					table2tablename_n = table2tablename_n + 1
					io.write(z_name.." = {\n"..x.."}\n")
				end
			end
		end
	end
end

print("argvmods = {")
for binary_name, rule in pairs(argvmods) do
	print(string.format("\t[\"%s\"] = {", binary_name))
	for name, value in pairs(rule) do
		if name == "name" then
			-- "name" field is redundant, skip it
		else
			io.write(string.format("\t%s = ", name))
			if type(value) == "string" then
				io.write(string.format("\"%s\"", value))
			elseif type(value) == "number" then
				io.write(string.format("%d", value))
			elseif type(value) == "table" then
				if #value == 0 then
					io.write("{}")
				else
					local x = stringlist_to_string(value)
					local z = name.."\n"..x
					if table2tablename[z] ~= nil then
						io.write(table2tablename[z])
					else
						io.write("{\n")
						io.write(x)
						io.write("\t}");
					end
				end
			else
				io.stderr:write(string.format(
				    "unsupported type '%s' in argvmod rule\n",
				    type(value)))
				assert(false)
			end
			io.write(",\n")
		end
	end
	print("\t},")
	count = count + 1
end
print("}")

print("--")
print(string.format("-- Total %d rule(s).", count))
print("-- End of rules created by argvmods expander.")
print("--")
