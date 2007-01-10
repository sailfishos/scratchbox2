-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>

install = {
	next = default_chain,
	binary = "^install$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

ln = {
	next = default_chain,
	binary = "^ln$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

cp = {
	next = default_chain,
	binary = "^cp$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

rm = {
	next = default_chain,
	binary = "^rm$",
	rules = {
		{path = ".*", map_to = "="}
	}
}

qemu = {
	next = default_chain,
	binary = ".*qemu.*",
	rules = {
		{path = "^/", map_to = "="}
	}
}

perl = {
	next = default_chain,
	binary = ".*perl.*",
	rules = {
		{path = "^/usr/lib/perl.*", map_to = nil}
	}
}

export_chains = {
	install,
	ln,
	cp,
	rm,
	qemu,
	perl
}
