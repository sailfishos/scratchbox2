--
-- Copyright (c) 2009 Nokia Corporation.
-- Copyright (c) 2021 Jolla Ltd.
-- License: MIT
--

--
-- Here is the necessary plumbing to generate argv&envp mangling
-- for miscellaneous binaries and wrappers
--
-- See syntax from argvenvp.lua.
--


local dirent = require 'posix.dirent'
local libgen = require 'posix.libgen'

dpkg_architecture = {
	name = "dpkg-architecture",
	path_prefixes = {"/usr/bin/"},
	remove = {"-f"},
}
argvmods[dpkg_architecture.name] = dpkg_architecture


for _,m_name in pairs(all_modes) do

   for file in dirent.files(session_dir.."/wrappers."..m_name) do
      if not (file == "..") and not (file == ".") then
         argvmods[file] = {
            name = file,
            --[[
               We don't check for the path of the particular wrapper but
               just search for the executeable to be wrapped in every path that could contain them.
               This way its simpler and supports targets that have gone under UsrMove
               and have merged bindirs.
            --]]
            path_prefixes = {"/usr/bin/",  "/bin/", "/sbin/", "/usr/sbin/"},
            new_filename = session_dir.."/wrappers."..m_name.."/"..file
         }
      end
   end

end
