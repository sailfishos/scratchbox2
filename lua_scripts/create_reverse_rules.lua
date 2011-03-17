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
-- 1. Reverse rules won't be created if the forward rules use "func_name"
-- conditions. It might be possible to fix that, but "func_names" certainly
-- complicate sorting of the generated reversing rules.

if not exec_engine_loaded then
	do_file(session_dir .. "/lua_scripts/argvenvp.lua")
end

allow_reversing = true	-- default = create reverse rules.
reversing_disabled_message = ""

-- Order of reverse rules is not necessarily the same as order of forward rules
function test_rev_rule_position(output_rules, d_path)
        local n
        for n=1,table.maxn(output_rules) do
		local rule = output_rules[n]
		local cmp_result
		cmp_result = sb.test_path_match(d_path,
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

function reverse_conditional_actions(output_rules, rev_rule_name, rule, n, forward_path)
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
		reverse_one_rule_xxxx(output_rules, actions[a], a, forward_path)
		actions[a].prefix = nil
		actions[a].path = nil
		actions[a].dir = nil
		actions[a].name = nil
	end
end

function reverse_one_rule(output_rules, rule, n)
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
		reverse_one_rule_xxxx(output_rules, rule, n, forward_path)
end

function reverse_one_rule_xxxx(output_rules, rule, n, forward_path)

		local new_rule = {}
		new_rule.comments = {}

		if rule.name then
			new_rule.name = string.format(
				"Rev: %s <%d>", rule.name, n)
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
				"Rev: %s <%d>", auto_name, n)
		end


		if (forward_path == nil) then
			new_rule.error = string.format(
				"--ERROR: Rule '%s' does not contain 'prefix' 'dir' or 'path'",
				new_rule.name)
		end

		if (rule.func_name ~= nil) then
			if (rule.path == "/") then
				-- allow reversion of func_name rules
				-- for the root directory:
			else
				allow_reversing = false
				reversing_disabled_message = string.format(
					"Rule '%s' has 'func_name' attribute",
					new_rule.name)
			end
		end

		local d_path = nil
		if (rule.use_orig_path) then
			new_rule.use_orig_path = true
			d_path = forward_path
		elseif (rule.force_orig_path) then
			new_rule.force_orig_path = true
			d_path = forward_path
		elseif (rule.actions) then
			reverse_conditional_actions(output_rules, new_rule.name,
				rule, n, forward_path)
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
			elseif (rule.dir) then
				new_rule.dir = d_path
				new_rule.orig_path = rule.dir
				idx = test_rev_rule_position(output_rules, d_path)
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

function reverse_rules(output_rules, input_rules)
        local n
        for n=1,table.maxn(input_rules) do
		local rule = input_rules[n]

		if rule.virtual_path then
			-- don't reverse virtual paths
			print("-- virtual_path set, not reversing", n)
		elseif rule.chain then
			reverse_rules(output_rules, rule.chain.rules)
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
		if (rule.dir) then
			print("\t dir=\""..rule.dir.."\",")
		end

		if (rule.use_orig_path) then
			print("\t use_orig_path=true,")
		end
		if (rule.force_orig_path) then
			print("\t force_orig_path=true,")
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
			reversing_disabled_message = rule.error
		end
		print("\t},")
	end
        print("-- Printed",table.maxn(rules),"rules")
end

function process_chains(chains_table)
        local n

	-- ensure that all chains have names
        for n=1,table.maxn(chains_table) do
		if chains_table[n].name then
			chains_table[n].rev_name = string.format(
				"reverse_chain__%s", chains_table[n].name)
		else
			chains_table[n].rev_name = string.format(
				"reverse_chain_%d", n)
		end
	end

        for n=table.maxn(chains_table),1,-1 do
		if chains_table[n].noentry then
			print("-- ========== ",n)
			print("-- noentry")
		else
			print(string.format("-- ======= %s =======",
				chains_table[n].rev_name))
			print(string.format("%s = {", chains_table[n].rev_name))

			if chains_table[n].next_chain then
				print(string.format("    next_chain=%s,",
					chains_table[n].next_chain.rev_name))
			else
				print("    next_chain=nil,")
			end

			if chains_table[n].binary then
				print(string.format("    binary=\"%s\",",
					chains_table[n].binary,"\n"))
			else
				print("    binary=nil,")
			end

			local output_rules = {}
			local rev_rules = reverse_rules(output_rules, chains_table[n].rules)
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
		print("-- Failed to create reverse rules (" ..
			reversing_disabled_message .. ")")
		print("reverse_chains = nil")
	end
end

print("-- Reversed rules from "..rule_file_path)
process_chains(active_mode_mapping_rule_chains)

