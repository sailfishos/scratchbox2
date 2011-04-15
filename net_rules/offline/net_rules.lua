-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "offline" networking rules: deny everything.

net_rule_file_interface_version = "100"
---------------------------------------

ipv4_rules = {
	{func_name = "connect", deny = true, errno = "EPERM"},
	{func_name = "bind", deny = true, errno = "EADDRNOTAVAIL"},
	-- 
	{deny = true, errno = "EPERM"} 
}

net_rules = {
	ipv4_out = ipv4_rules,
	ipv4_in = ipv4_rules,
}
