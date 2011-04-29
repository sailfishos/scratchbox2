-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "offline" networking rules: deny everything.

net_rule_file_interface_version = "100"
---------------------------------------

deny_all_rules = {
	{func_name = "connect", deny = true, errno = "EPERM"},
	{func_name = "bind", deny = true, errno = "EADDRNOTAVAIL"},
	-- 
	{deny = true, errno = "EPERM"} 
}

net_rules = {
	ipv4_out = deny_all_rules,
	ipv4_in = deny_all_rules,

	ipv6_out = deny_all_rules,
	ipv6_in = deny_all_rules,
}
