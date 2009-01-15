-- Copyright (c) 2009 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed after a new SB2 session has been created,
-- to create mapping rules for toolchain components:
-- For example, /usr/bin/gcc will be mapped to the toolchain. These rules
-- are used for other filesystem operations than exec*
-- (for example, when the shell is looking for a program, it must be
-- possible to stat() the destination)
--

gcc_rule_file_path = session_dir .. "/gcc-conf.lua"

default_rule = os.getenv("SBOX_ARGVMODS_USR_BIN_DEFAULT_RULE")

function argvmods_to_mapping_rules()
	local n
	print("argvmods_rules_for_usr_bin = {")
	print(" rules = {")
	for n in pairs(argvmods) do
		local rule = argvmods[n]
		-- print("-- rule ", n, " new_filename=", rule.new_filename)
		local k
		for k=1,table.maxn(rule.path_prefixes) do
			if rule.path_prefixes[k] == "/usr/bin/" and
			   rule.new_filename ~= nil then
				-- this rule maps "n" from /usr/bin to
				-- another file
				if sb.path_exists(rule.new_filename) then
					print("  {path=\"/usr/bin/"..n.."\",")
					print("   replace_by=\"" ..
						rule.new_filename.."\"},")
				else
					print("  -- WARNING: " ..
						rule.new_filename ..
						" does not exist")
				end
			end
		end
	end
	if (default_rule ~= nil) then
		print("  -- default:")
		print("  {prefix=\"/usr/bin\",", default_rule, "}")
	end
	print(" }")
	print("}")
end

print("-- Argvmods-to-mapping-rules converter:")
print("-- Automatically generated mapping rules. Do not modify:")

do_file(session_dir .. "/lua_scripts/argvenvp.lua")
argvmods_to_mapping_rules()

print("-- End of rules created by argvmods-to-mapping-rules converter.")

