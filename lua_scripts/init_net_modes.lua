-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- License: MIT

-- This script is executed when SB2 session is created
-- (from init.lua) to load networking rules to the rule
-- tree database.

local net_rule_interface_version = "100"

net_rule_file_interface_version = nil
net_rules = nil

-- These must match #defines in rule_tree.h:
local SB2_RULETREE_NET_RULETYPE_DENY  = 0
local SB2_RULETREE_NET_RULETYPE_ALLOW = 1
local SB2_RULETREE_NET_RULETYPE_RULES = 2


function load_and_check_network_rules(net_modename)
	local network_rule_file_path = session_dir .. "/net_rules/" ..
		net_modename .. "/net_rules.lua"

	sblib.log("debug", "network_rule_file_path = "..network_rule_file_path)

	net_rule_file_interface_version = nil
	net_rules = nil

	do_file(network_rule_file_path)

	if (net_rule_file_interface_version == nil) or 
	   (type(net_rule_file_interface_version) ~= "string") then
		io.stderr:write(string.format(
			"Network rule file interface version check failed: "..
			"No version information in %s",
			network_rule_file_path))
		net_rules = nil
	end
	if net_rule_file_interface_version ~= net_rule_interface_version then
		io.stderr:write(string.format(
			"Netork rule file interface version check failed: "..
			"got %s, expected %s (%s)", rule_file_interface_version,
			net_rule_interface_version, network_rule_file_path))
		net_rules = nil
	end

	if (type(net_rules) ~= "table") then
		io.stderr:write(string.format(
			"'net_rules' is not a table (%s)",
			network_rule_file_path));
		net_rules = nil
	end

	sblib.log("debug", "network rules loaded.")
end

local valid_keywords_in_net_rules = {
	log_level = "string",
	log_msg = "string",

	allow = "boolean",
	deny = "boolean",

	address = "string",
	port = "number",
	new_address = "string",
	new_port = "number",
	func_name = "string",
	binary_name = "string",
	errno = "string",

	rules = "table",
}

function add_one_net_rule(net_modename, chain_name, rule)
	local rule_ok = true -- assume ok.

	-- Check the rule:
	for key,val in pairs(rule) do
		local required_type = valid_keywords_in_net_rules[key]
		local t = type(val)
		if required_type then
			if t ~= required_type then
				io.stderr:write(string.format(
					"net rules: Invalid type %s for keyword "..
					"%s, expected %s (%s,%s)\n",
					t, key, required_type, net_modename, chain_name))
				rule_ok = false
			end
		else
			io.stderr:write(string.format(
				"net rules: Invalid keyword"..
				" %s (%s,%s)\n",
				key, net_modename, chain_name))
			rule_ok = false
		end
	end

	local ruletype
	local subrules = 0
	if rule.allow then
		if rule.deny then
			io.stderr:write(string.format(
				"ERROR: network rule has 'allow' and 'deny', only one must be used (%s,%s)\n",
				net_modename, chain_name))
			rule_ok = false
		end
		if rule.rules then
			io.stderr:write(string.format(
				"ERROR: network rule has 'allow' and 'rules', only one must be used (%s,%s)\n",
				net_modename, chain_name))
			rule_ok = false
		end
		ruletype = SB2_RULETREE_NET_RULETYPE_ALLOW
	elseif rule.deny then
		if rule.rules then
			io.stderr:write(string.format(
				"ERROR: network rule has 'deny' and 'rules', only one must be used (%s,%s)\n",
				net_modename, chain_name))
			rule_ok = false
		end
		ruletype = SB2_RULETREE_NET_RULETYPE_DENY
	elseif rule.rules and type(rule.rules) == "table" then
		ruletype = SB2_RULETREE_NET_RULETYPE_RULES
		subrules = add_net_rule_chain(net_modename, chain_name.."->rules", rule.rules)
	else
		io.stderr:write(string.format(
			"ERROR: illegal network rule, it doesn't to have 'allow', 'deny' or 'rules' (%s,%s)",
			net_modename, chain_name))
		rule_ok = false
	end

	if rule_ok then
		print("Rule ok, add it")
		return ruletree.add_net_rule_to_ruletree(
			ruletype,
			ruletree.new_string(rule.func_name),
			ruletree.new_string(rule.binary_name),
			ruletree.new_string(rule.address),
			rule.port,
			ruletree.new_string(rule.new_address),
			rule.new_port,
			rule.log_level,
			ruletree.new_string(rule.log_msg),
			rule.errno,
			subrules)
	end
	print("Broken rule")
	return 0
end

function add_net_rule_chain(net_modename, chain_name, rules)
	print("-- add_net_rule_chain:")
	local rule_list_index = 0

	if rules ~= nil then
		local num_rules = #rules

		if num_rules > 0 then
			rule_list_index = ruletree.objectlist_create(num_rules)

			for n=1,#rules do
				local rule = rules[n]
				local new_rule_index

				if type(rule) == "table" then
					new_rule_index = add_one_net_rule(net_modename, chain_name, rule)
					ruletree.objectlist_set(rule_list_index, n-1, new_rule_index)
				else
					io.stderr:write(string.format(
						"rule format error (%s,%s) [%d]\n",
						net_modename, chain_name, n))
				end
			end
			print("-- Added to rule db: ",#rules,"rules, idx=", rule_list_index)
		else
			io.stderr:write(string.format(
				"empty net rule chain (%s,%s)\n",
				net_modename, chain_name))
		end
	else
		io.stderr:write(string.format(
			"no net rule chain (%s,%s)\n",
			net_modename, chain_name))
	end
	return rule_list_index
end

function add_network_rules(net_modename)
	print("Adding network rules:", net_modename)
	load_and_check_network_rules(net_modename)

	if net_rules then
		for chain_name,rules in pairs(net_rules) do
			print("Processing:", net_modename, chain_name)
			local chain_index
			chain_index = add_net_rule_chain(net_modename, chain_name, rules)
			ruletree.catalog_vset("NET_RULES", net_modename, chain_name,
				chain_index)
		end
	end
--        if (all_exec_policies ~= nil) then
--                for i = 1, table.maxn(all_exec_policies) do
--                        local ep_name = all_exec_policies[i].name
--			if ep_name then
--				sblib.log("debug", "Adding Exec policy "..ep_name)
--				for key,val in pairs(all_exec_policies[i]) do
--					local required_type = valid_keywords_in_exec_policy[key]
--					local t = type(val)
--					if required_type then
--						if t == required_type then
--							add_to_exec_policy(modename_in_ruletree, ep_name,
--								key, t, val)
--						elseif required_type == "MappingRules" and
--							t == "table" then
--							add_mapping_rules_to_exec_policy(
--								modename_in_ruletree, ep_name,
--								key, val)
--						else
--							io.stderr:write(string.format(
--								"exec policy loader: Invalid type %s for keyword "..
--								"%s in exec policy, expected %s (mode=%s, policy=%s)\n",
--								t, key, required_type, modename_in_ruletree, ep_name))
--						end
--					else
--						io.stderr:write(string.format(
--							"exec policy loader: Invalid keyword"..
--							" %s in exec policy (mode=%s, policy=%s)\n",
--							key, modename_in_ruletree, ep_name))
--					end
--				end
--			end
--                end
--        end
end

local default_net_mode = os.getenv("SB2_DEFAULT_NETWORK_MODE")
local all_net_modes_str = os.getenv("SB2_ALL_NET_MODES")

all_net_modes = {}

if (all_net_modes_str) then
	for m in string.gmatch(all_net_modes_str, "[^ ]*") do
		if m ~= "" then
			table.insert(all_net_modes, m)
			ruletree.catalog_set("NET_RULES", m, 0)
		end
	end
end
ruletree.catalog_set("NET_RULES", "#default", ruletree.new_string(default_net_mode))

for m_index,m_name in pairs(all_net_modes) do
	add_network_rules(m_name)
end
