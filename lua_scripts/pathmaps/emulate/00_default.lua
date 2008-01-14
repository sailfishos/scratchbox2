-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.


mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{match = ".*qemu.*", map_to = nil},
		{match = "^" .. esc_target_root .. ".*", map_to = nil},
		{match = "^/dev", map_to = nil},
		{match = "^/proc", map_to = nil},
		{match = "^/tmp", map_to = nil},
		{match = "^/sys", map_to = nil},
-- maemo sdk hack, allows access to users' $HOME dirs
-- to make installing from outside the buildroot possible
-- /home/user is special on maemo systems... just don't ask, OK?
		{match = "^/home/user.*", map_to = "="},
		{match = "^/usr/share/scratchbox2.*", map_to = nil},
		{match = "^/home.*", map_to = nil},

		{match = ".*", map_to = esc_target_root}
	}
}

export_chains = {
	mapall_chain
}
