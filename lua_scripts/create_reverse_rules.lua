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
-- FIXME:
-- 1. Rules with conditional actions are not reversed correctly. Basically,
-- this script just gives up and marks the destinations with "use_orig_path".
-- This should be fixed, even if it is not a problem with our current
-- "official" mapping modes.
--
-- 2. Reverse rules won't be created if the forward rules use "func_name"
-- conditions. It might be possible to fix that, but "func_names" certainly
-- complicate sorting of the generated reversing rules.
--
-- 3. Rule chains with "next_chain" set to something else than 'nil'
-- are not currently supported.

allow_reversing = true	-- default = create reverse rules.

-- Order of reverse rules is not necessarily the same as order of forward rules
function test_rev_rule_position(output_rules, d_path)
        local n
        for n=1,table.maxn(output_rules) do
		local rule = output_rules[n]
		if (rule.prefix and (rule.prefix ~= "") and
		    (isprefix(rule.prefix, d_path))) then
			return n
		end
		-- "path" rules: (exact match)
		if (rule.path == d_path) then
			return n
		end
	end
	return nil
end

function reverse_one_rule(output_rules, rule, n)
		local new_rule = {}
		new_rule.comments = {}

		if rule.name then
			new_rule.name = string.format(
				"Rev: %s (%d)", rule.name, n)
		else
			new_rule.name = string.format(
				"Rev: Rule %d", n)
		end

		local forward_path
		if (rule.prefix) then
			forward_path = rule.prefix
		elseif (rule.path) then
			forward_path = rule.path
		elseif (rule.dir) then
			forward_path = rule.dir
		else
			forward_path = nil
			new_rule.error = string.format(
				"--ERROR: Rule '%s' does not contain 'prefix' or 'path'",
				new_rule.name)
		end

		if (rule.func_name ~= nil) then
			allow_reversing = false
		end

		local d_path = nil
		if (rule.use_orig_path) then
			new_rule.use_orig_path = true
			d_path = forward_path
		elseif (rule.actions) then
			-- FIXME: To be implemented. See the "TODO" list at top.
			new_rule.use_orig_path = true
			d_path = forward_path
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
		else
			new_rule.error = string.format(
				"--ERROR: Rule '%s' does not contain any actions",
				new_rule.name)
		end

		local idx = nil
		if (d_path ~= nil) then
			if (rule.prefix) then
				new_rule.prefix = d_path
				new_rule.orig_prefix = rule.prefix
				idx = test_rev_rule_position(output_rules, d_path..":")
			elseif (rule.path) then
				new_rule.path = d_path
				new_rule.orig_path = rule.path
				idx = test_rev_rule_position(output_rules, d_path)
			end
		end

		if (idx ~= nil) then
			-- a conflict, must reorganize
			table.insert(new_rule.comments, string.format(
				"--NOTE: '%s' conflicts with '%s', reorganized",
				new_rule.name, output_rules[idx].name))

			local x_path = nil
			if (output_rules[idx].prefix) then
				x_path = output_rules[idx].prefix
			elseif (output_rules[idx].path) then
				x_path = output_rules[idx].path
			end

			table.insert(output_rules, idx, new_rule)
			
			if x_path then
				local idx2
				idx2 = test_rev_rule_position(output_rules, x_path)

				if idx2 ~= idx+1 then
					table.insert(new_rule.comments, string.format(
						"--NOTE: '%s' DOUBLE CONFLICT with '%s'",
						new_rule.name, output_rules[idx].name))
				end
			end

		else
			-- no conflicts
			table.insert(output_rules, new_rule)
		end
end

function reverse_rules(input_rules)
	local output_rules = {}
        local n
        for n=1,table.maxn(input_rules) do
		local rule = input_rules[n]

		if rule.virtual_path then
			-- don't reverse virtual paths
			print("-- virtual_path set, not reversing", n)
		elseif rule.chain then
			reverse_rules(rule.chain.rules)
		else
			reverse_one_rule(output_rules, rule, n)
		end

	end
	return(output_rules)
end

function print_rules(rules)
        local n
        for n=1,table.maxn(rules) do
		local rule = rules[n]

		print(string.format("\t{name=\"%s\",", rule.name))

		local k
		for k=1,table.maxn(rule.comments) do
			print(rule.comments[k])
		end

		if (rule.orig_prefix) then
			print("\t -- orig_prefix", rule.orig_prefix)
		end
		if (rule.orig_path) then
			print("\t -- orig_path", rule.orig_path)
		end

		if (rule.prefix) then
			print("\t prefix=\""..rule.prefix.."\",")
		end
		if (rule.path) then
			print("\t path=\""..rule.path.."\",")
		end

		if (rule.use_orig_path) then
			print("\t use_orig_path=true,")
		end
		-- FIXME: To be implemented. See the "TODO" list at top.
		-- elseif (rule.actions) then
		--	print("\t -- FIXME: handle 'actions'")
		--	print(string.format(
		--		"\t %s=\"%s\",\n\t use_orig_path=true},",
		--		sel, fwd_target))
		if (rule.map_to) then
			print("\t map_to=\""..rule.map_to.."\",")
		end
		if (rule.replace_by) then
			print("\t replace_by=\""..rule.replace_by.."\",")
		end
		if (rule.error) then
			print("\t -- ",rule.error)
			allow_reversing = false
		end
		print("\t},")
	end
end

function process_chains(chains_table)
        local n

        for n=1,table.maxn(chains_table) do
		if chains_table[n].noentry then
			print("-- ========== ",n)
			print("-- noentry")
		else
			print(string.format("-- ======= Chain %d =======",n))
			print(string.format("reverse_chain_%d = {",n))

			if chains_table[n].next_chain then
				-- FIXME: Handle next_chain!!!
				print("    -- NOTE: next_chain is not nil,")
				print("    -- can't create reversing rules")
				allow_reversing = false
			else
				print("    next_chain=nil,")
			end

			if chains_table[n].binary then
				print(string.format("    binary=\"%s\",",
					chains_table[n].binary,"\n"))
			else
				print("    binary=nil,")
			end

			local rev_rules = reverse_rules(chains_table[n].rules)
			if (allow_reversing) then
				print("    rules={")
				print_rules(rev_rules)
				print("    }")
			end
			print("}")
		end
	end

	if (allow_reversing) then
		print("reverse_chains = {")
		for n=1,table.maxn(chains_table) do
			if chains_table[n].noentry then
				print(string.format("    -- %d = noentry",n))
			else
				print(string.format("    reverse_chain_%d,",n))
			end
		end
		print("}")
	else
		print("-- Failed to create reverse rules.")
		print("reverse_chains = nil")
	end
end

print("-- Reversed rules from "..rule_file_path)
process_chains(active_mode_mapping_rule_chains)

