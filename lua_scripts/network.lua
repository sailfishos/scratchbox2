-- Scratchbox2 network address translation
-- Copyright (C) 2011 Nokia Corporation.nen
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.

local forced_network_mode = sb.get_forced_network_mode()

local net_rule_interface_version = "100"

-- rule_file_path and rev_rule_file_path are global varibales
if forced_network_mode == nil then
	network_rule_file_path = session_dir .. "/net_rules/Default/net_rules.lua"
else
	network_rule_file_path = session_dir .. "/net_rules/"..
			forced_network_mode .. "/net_rules.lua"
end

function load_and_check_network_rules()
	sb.log("debug", "network_rule_file_path = "..network_rule_file_path)

	net_rule_file_interface_version = nil
	net_rules = nil

	do_file(network_rule_file_path)

	if (net_rule_file_interface_version == nil) or 
	   (type(net_rule_file_interface_version) ~= "string") then
		sb.log("error", string.format(
			"Fatal: Network rule file interface version check failed: "..
			"No version information in %s",
			network_rule_file_path))
		os.exit(79)
	end
	if net_rule_file_interface_version ~= net_rule_interface_version then
		sb.log("error", string.format(
			"Fatal: Netork rule file interface version check failed: "..
			"got %s, expected %s", rule_file_interface_version,
			net_rule_interface_version))
		os.exit(78)
	end

	if (type(net_rules) ~= "table") then
		sb.log("error", "'net_rules' is not an array.");
		os.exit(77)
	end

	sb.log("debug", "network rules loaded.")
end

function find_net_rule(netruletable, realfnname, addr_type,
	orig_dst_addr, orig_port, binary_name)

	for i = 1, table.maxn(netruletable) do
		local rule = netruletable[i]

		if rule and rule.port and rule.port ~= orig_port then
			rule = nil
		end
		
		if rule and rule.func_name then
			if rule.func_name == realfnname then
				sb.log("noise", "func_name ok in net_rule")
			else
				rule = nil
			end
		end
		
		if rule and rule.binary_name then
			if rule.binary_name == realfnname then
				sb.log("noise", "binary_name ok in net_rule")
			else
				rule = nil
			end
		end

		if rule and rule.address then
			res,msg = sb.test_net_addr_match(addr_type,
				orig_dst_addr, rule.address)
			if res then
				sb.log("noise", "address ok in net_rule, "..msg)
			else
				sb.log("noise", "address test failed; "..msg)
				rule = nil
			end
		end

		if rule and rule.rules then
			return find_net_rule(rule.rules, realfnname, addr_type,
				orig_dst_addr, orig_port, binary_name)
		end

		if rule then
			return rule
		end
	end
end

function sbox_map_network_addr(realfnname, protocol, addr_type,
	orig_dst_addr, orig_port, binary_name)

	-- FIXME: Parameter "protocol" is currently unused. See
	-- the related parts in preload/network.c and luaif/network.c
	-- for details.

	sb.log("debug",
		"sbox_map_network_addr: "..realfnname.."/"..addr_type..
		": addr="..orig_dst_addr.." port="..orig_port..
		" ("..binary_name..")")

	rule_list = net_rules[addr_type]
	if rule_list then
		rule = find_net_rule(rule_list, realfnname, addr_type,
			orig_dst_addr, orig_port, binary_name)

		dst_port = orig_port
		dst_addr = orig_dst_addr

		if rule.new_port then
			dst_port = rule.new_port
			sb.log("debug", "network port set to "..
				dst_port.." (was "..orig_port..")")
		end
		if rule.new_address then
			dst_addr = rule.new_address
			sb.log("debug", "network addr set to "..
				dst_addr.." (was "..orig_dst_addr..")")
		end

		if rule.allow then
			if rule.deny then
				sb.log("error", "network rule has both 'allow' and 'deny'")
				return "EPERM", orig_dst_addr, orig_port,
					rule.log_level, rule.log_msg
			end
			return "OK", dst_addr, dst_port, 
					rule.log_level, rule.log_msg
		else
			if not rule.deny then
				sb.log("error", "network rule must define either 'allow' or"..
					" 'deny', this rule has neither")
				return "EPERM", orig_dst_addr, orig_port,
					rule.log_level, rule.log_msg
			end
			return "ENETUNREACH", orig_dst_addr, orig_port,
					rule.log_level, rule.log_msg
		end
	end

	sb.log("warning", "No network rules for addr_type '"..
		addr_type.."', operation denied.")
	return "EPERM", orig_dst_addr, orig_port, nil, nil
end

load_and_check_network_rules()

