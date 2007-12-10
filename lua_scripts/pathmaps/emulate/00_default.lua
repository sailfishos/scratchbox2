-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.


mapall_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = ".*qemu.*", map_to = nil},
		{path = "^" .. escape_string(target_root) .. ".*", map_to = nil},
		{path = "^/dev", map_to = nil},
		{path = "^/proc", map_to = nil},
		{path = "^/tmp", map_to = nil},
		{path = "^/sys", map_to = nil},
-- maemo sdk hack, allows access to users' $HOME dirs
-- to make installing from outside the buildroot possible
-- /home/user is special on maemo systems... just don't ask, OK?
		{path = "^/home/user.*", map_to = "="},
		{path = "^/usr/share/scratchbox2.*", map_to = nil},
		{path = "^/home.*", map_to = nil},

		{path = ".*", map_to = "="}
	}
}

export_chains = {
	mapall_chain
}
