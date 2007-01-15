-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT License.

install = {
	next_chain = default_chain,
	binary = "^install$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

ln = {
	next_chain = default_chain,
	binary = "^ln$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

cp = {
	next_chain = default_chain,
	binary = "^cp$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

rm = {
	next_chain = default_chain,
	binary = "^rm$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

qemu = {
	next_chain = default_chain,
	binary = ".*qemu.*",
	rules = {
		{path = "^/", map_to = "="}
	}
}


dpkg = {
	next_chain = default_chain,
	binary = ".*dpkg.*",
	rules = {
		{path = "^/usr/lib/dpkg.*", map_to = nil},
		{path = "^/usr/share/dpkg.*", map_to = nil}
	}
}

-- Some stuff is under share or even etc...
perl = {
	next_chain = dpkg,
	binary = ".*perl.*",
	rules = {
		{path = ".*perl.*", map_to = nil}
	}
}

-- fakeroot needs this
sh = {
	next_chain = default_chain,
	binary = ".*sh.*",
	rules = {
		{path = "^/usr/lib.*", map_to = nil}
	}
}

export_chains = {
	install,
	ln,
	cp,
	rm,
	qemu,
	dpkg,
	perl,
	sh
}
