-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT License.


util_chain = {
	next_chain = default_chain,
	binary = nil,
	noentry = 1,
	rules = {
		autoconf,
		automake,
		aclocal,
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = ".*", map_to = "="}
	}
}

install = {
	next_chain = util_chain,
	binary = "^install$"
}

ln = {
	next_chain = util_chain,
	binary = "^ln$"
}

cp = {
	next_chain = util_chain,
	binary = "^cp$"
}

rm = {
	next_chain = util_chain,
	binary = "^rm$"
}


libtool = {
	next_chain = default_chain,
	binary = ".*libtool.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}

qemu = {
	next_chain = default_chain,
	binary = ".*qemu.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}


dpkg = {
	next_chain = default_chain,
	binary = ".*dpkg.*",
	rules = {
		{path = "^" .. target_root ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
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
	util_chain,
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
