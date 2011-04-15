-- Copyright (c) 2011 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license.

-- "localhost" networking rules: Allow to local host, deny other destinations.
-- Changes INADDR_ANY to 127.0.0.1.

net_rule_file_interface_version = "100"
---------------------------------------

ipv4_rules_out = {
	{address = "127.0.0.0/8", allow = true},

	-- Anyone who tries to connect to or send to INADDR_ANY
	-- will be forced to use 127.0.0.1 instead.
	{address = "INADDR_ANY", allow = true, new_address = "127.0.0.1"},

	-- default rule
	{deny = true, errno = "EPERM"} 
}

ipv4_rules_in = {
	{address = "127.0.0.0/8", allow = true},

	-- Anyone who tries to receive from INADDR_ANY
	-- will be set to receive from the localhost address
	-- only.
	{address = "INADDR_ANY", allow = true, new_address = "127.0.0.1"},

	-- Defaults.
	{func_name = "bind", deny = true, errno = "EADDRNOTAVAIL"},
	{deny = true, errno = "EPERM"} 
}


net_rules = {
	ipv4_out = ipv4_rules_out,
	ipv4_in = ipv4_rules_in,
}
