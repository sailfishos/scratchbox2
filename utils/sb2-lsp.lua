#!/usr/bin/lua

stdlib = require 'posix.stdlib'

stdlib.setenv('SBOX_DISABLE_MAPPING', 1, true)
sb = require 'sb_pathmap'
stdlib.setenv('SBOX_DISABLE_MAPPING', nil, true)


target_usr = sb.path("/usr")
print(target_usr)
print(sb.reverse_path(target_usr))
stdlib.setenv('SBOX_DISABLE_MAPPING', nil, true)
os_release = io.popen("cat /etc/os-release")
io.input(os_release)
print(io.read())
os_release:close()


stdlib.setenv('SBOX_DISABLE_MAPPING', 1, true)
os_release = io.popen("cat /etc/os-release")
io.input(os_release)
print(io.read())
os_release:close()

-- print(stdlib.getenv('LD_LIBRARY_PATH'))
-- for a,b in pairs(stdlib.getenv()) do print(a, b) end
