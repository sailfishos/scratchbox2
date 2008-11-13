-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "16"
----------------------------------

sb1_compat_dir = sbox_target_root .. "/scratchbox1-compat"

-- Don't map the working directory where sb2 was started, unless
-- that happens to be the root directory.
if sbox_workdir == "/" then
	-- FIXME. There should be a way to skip a rule...
	unmapped_workdir = "/XXXXXX" 
else
	unmapped_workdir = sbox_workdir
end

-- use "==" to test options as long as there is only one possible option,
-- string.match() is slow..
if sbox_mode_specific_options == "use-global-tmp" then
	tmp_dir_dest = "/tmp"
else
	tmp_dir_dest = session_dir .. "/tmp"
end

-- If the permission token exists and contains "root", target_root
-- will be available in R/W mode. Otherwise it will be "mounted" R/O.
local target_root_is_readonly
if sb.get_session_perm() == "root" then
	target_root_is_readonly = false
else
	target_root_is_readonly = true
end

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{path = sbox_cputransparency_method, use_orig_path = true,
		 readonly = true},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 readonly = true},

		{prefix = target_root, use_orig_path = true,
		 readonly = target_root_is_readonly},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. sbox_mapmode,
		 readonly = true},

		-- Scratchbox 1 compatibility rules:
		{ prefix = "/targets/", map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
		{ path = "/usr/bin/scratchbox-launcher.sh",
                  map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
                { path = "/etc/osso-af-init/dbus-systembus.sh",
                  map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
		
		-- gdb wants to have access to our dynamic linker also.
		{path = "/usr/lib/libsb2/ld-2.5.so", use_orig_path = true,
		 readonly = true},

		--
		{prefix = "/var/run", map_to = session_dir},

		-- 
		{prefix = session_dir, use_orig_path = true},
		{prefix = "/tmp", replace_by = tmp_dir_dest},

		-- 
		{prefix = "/dev", use_orig_path = true},
		{prefix = "/proc", use_orig_path = true},
		{prefix = "/sys", use_orig_path = true},
		{prefix = sbox_user_home_dir .. "/.scratchbox2",
		 use_orig_path = true},
		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		{prefix = "/etc/resolv.conf", use_orig_path = true,
		 readonly = true},

		-- -----------------------------------------------
		-- "user" is a special username, and should not be mapped:
		{prefix = "/home/user", map_to = target_root,
		 readonly = target_root_is_readonly},
		-- Other home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},
		-- -----------------------------------------------

		-- The default is to map everything to target_root,
		-- except that we don't map the directory tree where
		-- sb2 was started.
		{prefix = unmapped_workdir, use_orig_path = true},

		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = target_root,
		 readonly = target_root_is_readonly}
	}
}

export_chains = {
	mapall_chain
}

-- Exec policy rules.

default_exec_policy = {
	name = "Default"
}

-- For target binaries:
-- First, note that "foreign" binaries are easy to handle, no problem there.
-- But if CPU transparency method has not been set, then host CPU == target CPU:
-- we have "target's native" and "host's native" binaries, that would look 
-- identical (and valid!) to the kernel. But they need to use different 
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

emulate_mode_target_ld_so = nil		-- default = not needed
emulate_mode_target_ld_library_path = nil	-- default = not needed

-- used if libsb2.so is not available in target_root:
emulate_mode_target_ld_library_path_suffix = nil

if (conf_target_sb2_installed) then
	if (conf_target_ld_so ~= nil) then
		-- use dynamic libraries from target, 
		-- when executing native binaries!
		emulate_mode_target_ld_so = conf_target_ld_so
		emulate_mode_target_ld_library_path = conf_target_ld_so_library_path

		-- FIXME: This exec policy should process (map components of)
		-- the current value of LD_LIBRARY_PATH, and add the results
		-- to emulate_mode_target_ld_library_path just before exec.
		-- This has not been done yet.
	end
else
	emulate_mode_target_ld_library_path_suffix = conf_target_ld_so_library_path
end

local exec_policy_target = {
	name = "Rootstrap",
	native_app_ld_so = emulate_mode_target_ld_so,
	native_app_ld_so_supports_argv0 = conf_target_ld_so_supports_argv0,
	native_app_ld_library_path = emulate_mode_target_ld_library_path,

	native_app_ld_library_path_suffix = emulate_mode_target_ld_library_path_suffix,

	native_app_locale_path = conf_target_locale_path,
	native_app_message_catalog_prefix = conf_target_message_catalog_prefix,
}

-- Note that the real path (mapped path) is used when looking up rules!
all_exec_policies_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
                -- the home directory is expected to contain target binaries:
                {prefix = sbox_user_home_dir, exec_policy = exec_policy_target},

		-- Target binaries:
		{prefix = target_root, exec_policy = exec_policy_target},


		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy = default_exec_policy}
	}
}

exec_policy_chains = {
	all_exec_policies_chain
}

