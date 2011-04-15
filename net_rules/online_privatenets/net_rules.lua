-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "online_privatenets" networking rules:
-- Allow to private address spaces and local host,
-- deny other destinations.

net_rule_file_interface_version = "100"
---------------------------------------

ipv4_rules = {
	-- Private address spaces:
	{address = "10.0.0.0/8", allow = true},
	{address = "172.16.0.0/12", allow = true},
	{address = "192.168.0.0/16", allow = true},

	-- localhost:
	{address = "127.0.0.0/8", allow = true},

	-- default rules.
	{func_name = "connect", deny = true, errno = "EPERM"},
	{func_name = "bind", deny = true, errno = "EADDRNOTAVAIL"},
	-- 
	{deny = true, errno = "EPERM"} 
}

net_rules = {
	ipv4_out = ipv4_rules,
	ipv4_in = ipv4_rules,
}
