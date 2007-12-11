-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- if tool_root ends up being nil, that's ok
tool_root = os.getenv("MAEMO_TOOLS")

simple_chain = {
	next_chain = nil,
	binary = ".*",
	rules = {
		{path = "^/lib.*", map_to = "="},
-- /usr/share is mapped to the buildroot except these directories
-- (taken from bora rootstrap contents)
		{path = "^/usr/share/aclocal.*", map_to = "="},
		{path = "^/usr/share/alsa.*", map_to = "="},
		{path = "^/usr/share/applications.*", map_to = "="},
		{path = "^/usr/share/apt.*", map_to = "="},
		{path = "^/usr/share/backgrounds.*", map_to = "="},
		{path = "^/usr/share/base.files.*", map_to = "="},
		{path = "^/usr/share/bug.*", map_to = "="},
		{path = "^/usr/share/certs.*", map_to = "="},
		{path = "^/usr/share/dbus.1.*", map_to = "="},
		{path = "^/usr/share/debconf.*", map_to = "="},
--autoscripts		{path = "^/usr/share/debhelper.*", map_to = "="},
		{path = "^/usr/share/defoma.*", map_to = "="},
		{path = "^/usr/share/dict.*", map_to = "="},
		{path = "^/usr/share/dpkg.*", map_to = "="},
		{path = "^/usr/share/docbook.utils.*", map_to = tool_root},
		{path = "^/usr/share/doc.*", map_to = "="},
--trippletable error if taken from target: {path = "^/usr/share/dpkg.*", map_to = "="},
		{path = "^/usr/share/fonts.*", map_to = "="},
		{path = "^/usr/share/glib.*", map_to = "="},
		{path = "^/usr/share/gnupg.*", map_to = "="},
		{path = "^/usr/share/gtk.doc/data/gtkdoc.common*", map_to = tool_root},
		{path = "^/usr/share/gtk.doc/data/gtk.doc.make*", map_to = tool_root},
		{path = "^/usr/share/gtk.doc/data/.*xsl", map_to = tool_root},
		{path = "^/usr/share/gtk.doc.*", map_to = "="},
--gtk-doc.make error if taken from target: {path = "^/usr/share/gtk.*", map_to = "="},
		{path = "^/usr/share/i18n.*", map_to = "="},
		{path = "^/usr/share/icons.*", map_to = "="},
		{path = "^/usr/share/ifupdown.*", map_to = "="},
		{path = "^/usr/share/info.*", map_to = "="},
		{path = "^/usr/share/initscripts.*", map_to = "="},
		{path = "^/usr/share/lintian.*", map_to = "="},
		{path = "^/usr/share/locale.*", map_to = "="},
		{path = "^/usr/share/man.*", map_to = "="},
		{path = "^/usr/share/mime.*", map_to = "="},
--readline		{path = "^/usr/share/misc.*", map_to = "="},
		{path = "^/usr/share/osso.*", map_to = "="},
		{path = "^/usr/share/passwd.*", map_to = "="},
		{path = "^/usr/share/ppp.*", map_to = "="},
--docbook		{path = "^/usr/share/sgml.*", map_to = "="},
		{path = "^/usr/share/sounds.*", map_to = "="},
		{path = "^/usr/share/sysklogd.*", map_to = "="},
		{path = "^/usr/share/sysvinit.*", map_to = "="},
		{path = "^/usr/share/tabset.*", map_to = "="},
		{path = "^/usr/share/telepathy.*", map_to = "="},
		{path = "^/usr/share/terminfo.*", map_to = "="},
		{path = "^/usr/share/themes.*", map_to = "="},
		{path = "^/usr/share/user.icons.*", map_to = "="},
		{path = "^/usr/share/X11.*", map_to = "="},
--docbook/stylesheet...		{path = "^/usr/share/xml.*", map_to = "="},
		{path = "^/usr/share/zoneinfo.*", map_to = "="},
		{path = "^/usr/bin/xml2.conf.*", map_to = "="},
--glib-mkenums error: points to /scratchbox/tools/bin/perl {path = "^/usr/bin/glib.*", map_to = "="},
		{path = "^/usr/bin/gobject.query.*", map_to = "="},
		{path = "^/usr/lib/perl.*", map_to = tool_root},
		{path = "^/usr/lib/dpkg.*", map_to = tool_root},
		{path = "^/usr/lib/cdbs.*", map_to = tool_root},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/include.*", map_to = "="},
		{path = "^/usr/share/aclocal.*", map_to = "="},
		{path = "^/var/.*/apt.*", map_to = "="},
		{path = "^/var/.*/dpkg.*", map_to = "="},
		{path = "^/host_usr", map_to = "="},
		{path = "^/tmp", map_to = nil},
		{path = "^/dev", map_to = nil},
		{path = "^/proc", map_to = nil},
		{path = "^/sys", map_to = nil},
		{path = ".*", map_to = tool_root}
	}
}

qemu_chain = {
	next_chain = nil,
	binary = ".*qemu.*",
	rules = {
		{path = "^/lib.*", map_to = "="},
		{path = "^/usr/lib.*", map_to = "="},
		{path = "^/usr/local/lib.*", map_to = "="},
		{path = "^/tmp", map_to = nil},
		{path = "^/dev", map_to = nil},
		{path = "^/proc", map_to = nil},
		{path = "^/sys", map_to = nil},
		{path = ".*", map_to = tool_root}
	}
}

dpkg_chain = {
	next_chain = simple_chain,
	binary = ".*",
	rules = {
		{path = "^/var/dpkg.*", map_to = "="},
		{path = "^/var/lib/dpkg.*", map_to = "="}
	}
}

apt_chain = {
	next_chain = simple_chain,
	binary = ".*apt.*",
	rules = {
		{path = "^" .. escape_string(target_root) .. ".*", map_to = tool_root},
		{path = "^/var/lib/apt.*", map_to = "="},
		{path = "^/var/cache/apt.*", map_to = "="},
		{path = "^/usr/lib/apt.*", map_to = tool_root},
		{path = "^/etc/apt.*", map_to = "="}
	}
}


-- fakeroot needs this
sh_chain = {
	next_chain = simple_chain,
	binary = ".*sh.*",
	rules = {
		{path = "^/usr/lib.*la", map_to = "="},
		{path = "^/usr/lib.*", map_to = tool_root},
	}
}

export_chains = {
	qemu_chain,
	sh_chain,
	apt_chain,
	simple_chain
}
