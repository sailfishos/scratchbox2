-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "online" networking rules: Allow all destinations.

net_rule_file_interface_version = "100"
---------------------------------------

ipv4_rules = {
	{allow = true}
}

net_rules = {
	ipv4_out = ipv4_rules,
	ipv4_in = ipv4_rules,
}
