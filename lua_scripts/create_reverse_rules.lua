-- Copyright (c) 2008 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed once after a new SB2 session has been created,
-- to create reversing rules for path mapping (see utils/sb2).
-- This is still simple, and may not work in all cases: This script 
-- will shut down if problems are detected (then path reversing won't be
-- available and SB2 works just as it did before this feature was implemented)
--

local allow_reversing = true	-- default = create reverse rules.
local reversing_disabled_message = ""

-- Order of reverse rules is not necessarily the same as order of forward rules
function test_rev_rule_position(output_rules, d_path)
        local n
        for n=1,table.maxn(output_rules) do
		local rule = output_rules[n]
		local cmp_result
		cmp_result = sblib.test_path_match(d_path,
			rule.dir, rule.prefix, rule.path)
		if (cmp_result >= 0) then
			return n
		end
	end
	return nil
end

function is_identical_reverse_rule(r1,r2)
	if ((r1.prefix == r2.prefix) and
	    (r1.path == r2.path) and
	    (r1.dir == r2.dir) and
	    (r1.replace_by == r2.replace_by)) then
		return true
	end
	return false
end

function reverse_conditional_actions(output_rules, rev_rule_name, rule, n, forward_path, modename)
	local actions = rule.actions

	local a
        for a = 1, table.maxn(actions) do
		-- actions are only partial rules; the "selector" is in
		-- "rule", but we must copy it temporarily to action[a],
		-- otherwise reverse_one_rule_xxxx() won't be able to
		-- process it completely. Also, invent a better 
		-- temporary name for it.
		actions[a].prefix = rule.prefix
		actions[a].path = rule.path
		actions[a].dir = rule.dir
		actions[a].name = string.format("%s/Act.%d", rev_rule_name, a)
		reverse_one_rule_xxxx(output_rules, actions[a], a, forward_path, modename)
		actions[a].prefix = nil
		actions[a].path = nil
		actions[a].dir = nil
		actions[a].name = nil
	end
end

function reverse_one_rule(output_rules, rule, n, modename)
		local forward_path
		if (rule.prefix) then
			forward_path = rule.prefix
		elseif (rule.path) then
			forward_path = rule.path
		elseif (rule.dir) then
			forward_path = rule.dir
		else
			forward_path = nil
		end
		reverse_one_rule_xxxx(output_rules, rule, n, forward_path, modename)
end

function reverse_one_rule_xxxx(output_rules, rule, n, forward_path, modename)

		local new_rule = {}
		new_rule.comments = {}

		if rule.name then
			new_rule.name = string.format(
				"Rev(%s): %s <%d>", modename, rule.name, n)
		else
			local auto_name = "??"
			if (rule.prefix) then
				auto_name = "prefix="..rule.prefix
			elseif (rule.dir) then
				auto_name = "dir="..rule.dir
			elseif (rule.path) then
				auto_name = "path="..rule.path
			end
			new_rule.name = string.format(
				"Rev(%s): %s <%d>", modename, auto_name, n)
		end


		if (forward_path == nil) then
			new_rule.error = string.format(
				"--ERROR: Rule '%s' does not contain 'prefix' 'dir' or 'path'",
				new_rule.name)
		end

		if (rule.func_name ~= nil) then
			table.insert(new_rule.comments, string.format(
				"--NOTE: orig.rule '%s' had func_name requirement '%s'",
				new_rule.name, rule.func_name))
		end

		local d_path = nil
		if (rule.use_orig_path) then
			new_rule.use_orig_path = true
			d_path = forward_path
		elseif (rule.force_orig_path) then
			new_rule.force_orig_path = true
			d_path = forward_path
		elseif (rule.force_orig_path_unless_chroot) then
			new_rule.force_orig_path_unless_chroot = true
			d_path = forward_path
		elseif (rule.actions) then
			reverse_conditional_actions(output_rules, new_rule.name,
				rule, n, forward_path, modename)
			return
		elseif (rule.map_to) then
			d_path = rule.map_to .. forward_path
			new_rule.replace_by = forward_path
		elseif (rule.replace_by) then
			d_path = rule.replace_by
			new_rule.replace_by = forward_path
		elseif (rule.custom_map_funct) then
			new_rule.error = string.format(
				"--Notice: custom_map_funct rules can't be reversed, please mark it 'virtual'",
				new_rule.name)
		elseif (rule.if_exists_then_map_to) then
			d_path = rule.if_exists_then_map_to .. forward_path
			new_rule.replace_by = forward_path
		elseif (rule.if_exists_then_replace_by) then
			d_path = rule.if_exists_then_replace_by
			new_rule.replace_by = forward_path
		elseif (rule.if_exists_in) then
			d_path = rule.if_exists_in
			new_rule.replace_by = forward_path
		elseif (rule.if_env_var_is_not_empty) then
			table.insert(new_rule.comments, string.format(
				"-- WARNING: Skipping 'if_env_var_is_not_empty' rule\t%d\n", n))
			new_rule.optional_rule = true
		elseif (rule.if_env_var_is_empty) then
			table.insert(new_rule.comments, string.format(
				"-- WARNING: Skipping 'if_env_var_is_empty' rule\t%d\n", n))
		else
			new_rule.error = string.format(
				"--ERROR: Rule '%s' does not contain any actions",
				new_rule.name)
			new_rule.optional_rule = true
		end

		local idx = nil
		if (d_path ~= nil) then
			if (rule.prefix) then
				new_rule.prefix = d_path
				new_rule.orig_prefix = rule.prefix
				idx = test_rev_rule_position(output_rules, d_path..":")
			elseif (rule.dir) then
				new_rule.dir = d_path
				new_rule.orig_path = rule.dir
				idx = test_rev_rule_position(output_rules, d_path)
			elseif (rule.path) then
				if (rule.path == "/") then
					-- Root directory rule.
					if rule.map_to then
						new_rule.path = rule.map_to
					else
						new_rule.path = d_path
					end
				else
					new_rule.path = d_path
				end
				new_rule.orig_path = rule.path
				idx = test_rev_rule_position(output_rules, d_path)
			end
		end

		if (idx ~= nil) then
			-- a conflict, must reorganize
			table.insert(new_rule.comments, string.format(
				"--NOTE: '%s' conflicts with '%s', reorganized",
				new_rule.name, output_rules[idx].name))
			if (is_identical_reverse_rule(new_rule,output_rules[idx])) then
				table.insert(output_rules[idx].comments,
					string.format("--NOTE: Identical rule '%s' generated (dropped)",
					new_rule.name))
			else
				local x_path = nil
				local older_rule = output_rules[idx]
				if (older_rule.prefix) then
					x_path = older_rule.prefix
				elseif (older_rule.dir) then
					x_path = older_rule.dir
				elseif (older_rule.path) then
					x_path = older_rule.path
				end

				if x_path then
					local dummy_rules = {}
					dummy_rules[1] = new_rule
					dummy_rules[2] = older_rule
					local idx2
					idx2 = test_rev_rule_position(dummy_rules, x_path)

					if idx2 ~= 2 then
						-- x_path (selector for the older rule)
						-- hit the new_rule, not the older rule.
						-- Two targets were mapped to the same place,
						-- can't reverse one path to two locations..
						table.insert(older_rule.comments, string.format(
							"--NOTE: '%s' WOULD CONFLICT with '%s', conflicting rule dropped",
							new_rule.name, older_rule.name))
					else
						table.insert(output_rules, idx, new_rule)
					end
				else
					table.insert(output_rules, idx, new_rule)
				end
			end
		else
			-- no conflicts
			table.insert(output_rules, new_rule)
		end
end

function reverse_rules(ofile, output_rules, input_rules, modename)
        local n
        for n=1,table.maxn(input_rules) do
		local rule = input_rules[n]

		if rule.virtual_path then
			-- don't reverse virtual paths
			ofile:write(string.format("-- virtual_path set, not reversing\t%d\n", n))
		elseif rule.rules then
			reverse_rules(ofile, output_rules, rule.rules, modename)
		elseif rule.union_dir then
			-- FIXME
			ofile:write(string.format("-- WARNING: Skipping union_dir rule\t%d\n", n))
		else
			reverse_one_rule(output_rules, rule, n, modename)
		end

	end
	return(output_rules)
end

function print_rules(ofile, rules)
        local n
        for n=1,table.maxn(rules) do
		local rule = rules[n]

		ofile:write(string.format("\t{name=\"%s\",\n", rule.name))

		local k
		for k=1,table.maxn(rule.comments) do
			ofile:write(rule.comments[k].."\n")
		end

		if (rule.orig_prefix) then
			ofile:write("\t -- orig_prefix\t"..rule.orig_prefix.."\n")
		end
		if (rule.orig_path) then
			ofile:write("\t -- orig_path\t".. rule.orig_path.."\n")
		end

		if (rule.prefix) then
			ofile:write("\t prefix=\""..rule.prefix.."\",\n")
		end
		if (rule.path) then
			ofile:write("\t path=\""..rule.path.."\",\n")
		end
		if (rule.dir) then
			ofile:write("\t dir=\""..rule.dir.."\",\n")
		end

		if (rule.use_orig_path) then
			ofile:write("\t use_orig_path=true,\n")
		end
		if (rule.force_orig_path) then
			ofile:write("\t force_orig_path=true,\n")
		end
		if (rule.force_orig_path_unless_chroot) then
			ofile:write("\t force_orig_path_unless_chroot=true,\n")
		end
		if (rule.binary_name) then
			ofile:write("\t binary_name=\""..rule.binary_name.."\",\n")
		end
		if (rule.optional_rule) then
			ofile:write("\t optional_rule=true,\n")
		end

		-- FIXME: To be implemented. See the "TODO" list at top.
		-- elseif (rule.actions) then
		--	ofile:write("\t -- FIXME: handle 'actions'\n")
		--	ofile:write(string.format(
		--		"\t %s=\"%s\",\n\t use_orig_path=true},\n",
		--		sel, fwd_target))
		if (rule.map_to) then
			ofile:write("\t map_to=\""..rule.map_to.."\",\n")
		end
		if (rule.replace_by) then
			ofile:write("\t replace_by=\""..rule.replace_by.."\",\n")
		end
		if (rule.error) then
			ofile:write(string.format("\t -- \t%s\n",rule.error))
			allow_reversing = false
			reversing_disabled_message = rule.error
		end
		ofile:write("\t},\n")
	end
        ofile:write(string.format("-- Printed\t%d\trules\n",table.maxn(rules)))
end

for m_index,m_name in pairs(all_modes) do
	local autorule_file_path = session_dir .. "/rules_auto/" .. m_name .. ".usr_bin.lua"
	local rule_file_path = session_dir .. "/rules/" .. m_name .. ".lua"
        local rev_rule_filename = session_dir .. "/rev_rules/" ..
                 m_name .. ".lua"
        local output_file = io.open(rev_rule_filename, "w")

	allow_reversing = true	-- default = create reverse rules.
	reversing_disabled_message = ""

	local current_rule_interface_version = "105"

	-- rulefile will set these:
	rule_file_interface_version = nil
	fs_mapping_rules = nil

	-- rulefile expects to see this:
	active_mapmode = m_name

	-- Reload "constants", just to be sure:
	do_file(session_dir .. "/lua_scripts/rule_constants.lua")

	do_file(autorule_file_path)
	do_file(rule_file_path)

	-- fail and die if interface version is incorrect
        if (rule_file_interface_version == nil) or 
           (type(rule_file_interface_version) ~= "string") then
                io.stderr:write(string.format(
                        "Fatal: Rule file interface version check failed: "..
                        "No version information in %s",
                        rule_file_path))
                os.exit(89)
        end
        if rule_file_interface_version ~= current_rule_interface_version then
                io.stderr:write(string.format(
                        "Fatal: Rule file interface version check failed: "..
                        "got %s, expected %s", rule_file_interface_version,
                        current_rule_interface_version))
                os.exit(88)
        end

	if (type(fs_mapping_rules) ~= "table") then
                io.stderr:write("'fs_mapping_rule' is not an array.");
                os.exit(87)
        end

	output_file:write("-- Reversed rules from "..rule_file_path.."\n")

	local output_rules = {}
	local rev_rules = reverse_rules(output_file, output_rules, fs_mapping_rules, m_name)
	if (allow_reversing) then
		output_file:write("reverse_fs_mapping_rules={\n")
		print_rules(output_file, rev_rules)
		-- Add a final rule for the root directory itself.
		output_file:write("\t{\n")
		output_file:write("\t\tname = \"Final root dir rule\",\n")
		output_file:write("\t\tpath = \""..target_root.."\",\n")
		output_file:write("\t\treplace_by = \"/\"\n")
		output_file:write("\t},\n")
		output_file:write("}\n")
	else
		output_file:write("-- Failed to create reverse rules (" ..
			reversing_disabled_message .. ")\n")
		output_file:write("reverse_fs_mapping_rules = nil\n")
	end
        output_file:close()
end

--cleanup
rule_file_interface_version = nil
fs_mapping_rules = nil

