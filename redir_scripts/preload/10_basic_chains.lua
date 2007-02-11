-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT License.

install = {
	next_chain = default_chain,
	binary = "^install$",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = ".*", map_to = "="}
	}
}

ln = {
	next_chain = default_chain,
	binary = "^ln$",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = ".*", map_to = "="}
	}
}

cp = {
	next_chain = default_chain,
	binary = "^cp$",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = ".*", map_to = "="}
	}
}

rm = {
	next_chain = default_chain,
	binary = "^rm$",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = ".*", map_to = "="}
	}
}


libtool = {
	next_chain = default_chain,
	binary = ".*libtool.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}

qemu = {
	next_chain = default_chain,
	binary = ".*qemu.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}


dpkg = {
	next_chain = default_chain,
	binary = ".*dpkg.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
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
		{path = "^/usr/lib.*", map_to = nil},
	}
}

export_chains = {
	install,
	ln,
	cp,
	rm,
	libtool,
	qemu,
	dpkg,
	perl,
	sh
}
