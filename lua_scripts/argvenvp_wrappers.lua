-- Copyright (c) 2021 Jolla Ltd.
-- Author: Björn Bidar
--
-- Licensed under LGPL-2.1 License

local dirent = require 'posix.dirent'
local libgen = require 'posix.libgen'

for m_index,m_name in pairs(all_modes) do

   active_mapmode = m_name

   for _, file in pairs(dirent.dir(session_dir.."/wrappers."..active_mapmode)) do
      if not (file == "..") and not (file == ".") then
         argvmods[file] = {
            name = file,
            --[[
               We don't check for the path of the particular wrapper but
               just search for the executeable to be wrapped in every path that could contain them.
               This way its simpler and supports targets that have gone under UsrMove
               and have merged bindirs.
            --]]
            path_prefixes = {"/usr/bin",  "/bin", "/sbin", "/usr/sbin"},
            new_filename = session_dir.."/wrappers."..active_mapmode.."/"..file
         }
      end
   end

end
