-- Rule file interface version, mandatory.
--
fs_rule_lib_interface_version = "105"
----------------------------------

-- Workspace root from sbox' point of view is not the mount point but already
-- the immediate subdirectory of the root directory, which is always one of
-- "/home", "/Home", "/Users", "/[a-z]" or "/_*" (that's how the mount point
-- is derived from the workspace path on host), none of which is to be mapped.
-- Use home dir as a fallback for easier use.
sbox_user_workspace = string.match(os.getenv("SAILFISH_SDK_SRC1_MOUNT_POINT") or sbox_user_home_dir,
   "^/[^/]+")

-- http://stackoverflow.com/a/4991602/337649
function user_file_readable(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

function use_outside_path(path)
  table.insert(  emulate_mode_rules, 1,
    {dir = path, rules = { {dir = path, use_orig_path = true}, }, })
end

function replace_outside_path(path, replacement)
  table.insert(  emulate_mode_rules, 1,
    {dir = path, rules = { {dir = path, replace_by = replacement, virtual_path = true}, }, })
end

-- Allow user-defined rules to "overlay" what we've defined here.
-- What is in ~/.sbrules gets executed as if its contents were in this file, on this line.
function run_user_sbrules()
   -- We could call .sbrules via pcall to trap errors but that just hides them from the user
   -- pcall(dofile, home .. '/.sbrules')
   local sbrules = sbox_user_home_dir .. '/.sbrules'
   if user_file_readable(sbrules) then
      do_file(sbrules)
   end
end
