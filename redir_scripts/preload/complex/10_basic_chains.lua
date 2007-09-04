-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT License.


util_chain = {
	next_chain = default_chain,
	binary = nil,
	noentry = 1,
	rules = {
		actual_root,
		autoconf,
		automake,
		aclocal,
		intltool,
		debhelper,
		dbs,
		doc,
		tex,
		sgml,
		xml,
		linuxdoc,
		bison,
		{path = "^" .. escape_string(target_root) .. ".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/tmp.*", map_to = nil},
		{path = "^/usr/share/misc/config.*", map_to = nil},
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

file = {
	next_chain = default_chain,
	binary = "^file$",
	rules = {
		{path = "^/usr/share/file/.*", map_to = nil}
	}
}

libtool = {
	next_chain = default_chain,
	binary = ".*libtool.*",
	rules = {
		{path = "^" .. escape_string(target_root) ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/tmp.*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}

qemu = {
	next_chain = default_chain,
	binary = ".*qemu.*",
	rules = {
		{path = "^" .. escape_string(target_root) ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/tmp.*", map_to = nil},
		{path = "^/", map_to = "="}
	}
}

dpkg = {
	next_chain = default_chain,
	binary = ".*dpkg.*",
	rules = {
		{path = "^" .. escape_string(target_root) ..".*", map_to = nil},
		{path = "^/home.*", map_to = nil},
		{path = "^/usr/lib/dpkg.*", map_to = nil},
		{path = "^/usr/share/dpkg.*", map_to = nil},
		{path = "^/tmp.*", map_to = nil},
		{path = "^/etc/dpkg/.*", map_to = "="}
	}
}

apt = {
	next_chain = default_chain,
	binary = ".*apt.*",
	rules = {
		{path = "^" .. escape_string(target_root) .. ".*", map_to = nil},
		{path = "^/var/lib/apt.*", map_to = "="},
		{path = "^/var/cache/apt.*", map_to = "="},
		{path = "^/usr/lib/apt.*", map_to = nil},
		{path = "^/etc/apt.*", map_to = "="}
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
	file,
	libtool,
	qemu,
	dpkg,
	apt,
	perl,
	sh
}
