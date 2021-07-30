-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "online" networking rules: Allow all destinations.

net_rule_file_interface_version = "100"
---------------------------------------

allow_all_rules = {
	{allow = true}
}

net_rules = {
	ipv4_out = allow_all_rules,
	ipv4_in = allow_all_rules,

	ipv6_out = allow_all_rules,
	ipv6_in = allow_all_rules,
}
