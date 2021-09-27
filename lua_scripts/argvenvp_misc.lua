--
-- Copyright (c) 2009 Nokia Corporation.
-- Licensed under MIT license
--

--
-- Here is the necessary plumbing to generate argv&envp mangling
-- for miscellaneous binaries.
--
-- See syntax from argvenvp.lua.
--

dpkg_architecture = {
	name = "dpkg-architecture",
	path_prefixes = {"/usr/bin/"},
	remove = {"-f"},
}
argvmods[dpkg_architecture.name] = dpkg_architecture
