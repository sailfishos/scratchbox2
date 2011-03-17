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

local argvmods_source_file = session_dir .. "/lua_scripts/" ..
    os.getenv("SBOX_ARGVMODS_SOURCE_FILE")
do_file(argvmods_source_file)

local allowed_rulenames = {
	"name",
	"path_prefixes",
	"add_head",
	"add_tail",
	"remove",
	"new_filename",
	"disable_mapping",
	"drivers",
}

print("--")
print("-- Generator: create_argvmods_rules.lua")
print("-- Source file: " .. argvmods_source_file)
print("-- Automatically generated rules.  Do not modify:")
print("--")

local count = 0
for binary_name, rule in pairs(argvmods) do
	print(string.format("argvmods[\"%s\"] = {", binary_name))
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
		io.write(string.format("\t%s = ", name))
		if type(value) == "string" then
			io.write(string.format("\"%s\"", value))
		elseif type(value) == "number" then
			io.write(string.format("%d", value))
		elseif type(value) == "table" then
			if #value == 0 then
				io.write("{}")
			else
				io.write("{\n")
				for i = 1, #value do
					assert(type(value[i]) == "string")

					io.write(
					    string.format("\t\t\"%s\",\n",
					    value[i]))
				end
				io.write("\t}");
			end
		else
			io.stderr:write(string.format(
			    "unsupported type '%s' in argvmod rule\n",
			    type(value)))
			assert(false)
		end
		io.write(",\n")
	end
	print("}")
	count = count + 1
end

print("--")
print(string.format("-- Total %d rule(s).", count))
print("-- End of rules created by argvmods expander.")
print("--")
