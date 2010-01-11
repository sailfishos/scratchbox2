-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "23"
----------------------------------

sb1_compat_dir = sbox_target_root .. "/scratchbox1-compat"
--
-- scratchbox1-compat is symlink that points to
-- the real sb1 compat directory.  To avoid mapping
-- problems later, we resolve this symlink right now.
--
local resolved_compat_dir = sb.readlink(sb1_compat_dir)
if resolved_compat_dir ~= nil then
	sb1_compat_dir = resolved_compat_dir
end

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
local fakeroot_ld_preload = ""
if sb.get_session_perm() == "root" then
	target_root_is_readonly = false
	fakeroot_ld_preload = ":"..host_ld_preload_fakeroot
else
	target_root_is_readonly = true
end

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

rootdir_rules = {
	rules = {
		{path = "/", func_name = ".*stat.*",
                    map_to = target_root, readonly = target_root_is_readonly },
		{path = "/", func_name = ".*open.*",
                    map_to = target_root, readonly = target_root_is_readonly },
		{path = "/", use_orig_path = true},
	}
}

mapall_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		{dir = session_dir, use_orig_path = true},

		{path = conf_tools_ld_so, use_orig_path = true, readonly = true},

		{path = sbox_cputransparency_method, use_orig_path = true,
		 readonly = true},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 readonly = true},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, readonly = true},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 readonly = true},

		{prefix = target_root, use_orig_path = true,
		 readonly = target_root_is_readonly},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{prefix = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 readonly = true},

		-- 
		-- Scratchbox 1 compatibility rules:
		-- Note that when you add/remove these, check
		-- also that dpkg_chain rules match these.
		--
		{ prefix = "/targets/", map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
		{ path = "/usr/bin/scratchbox-launcher.sh",
                  map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
                { path = "/etc/osso-af-init/dbus-systembus.sh",
                  map_to = sb1_compat_dir,
		  readonly = target_root_is_readonly},
		-- "policy-rc.d" checks if scratchbox-version exists, 
		-- to detect if it is running inside scratchbox..
		{prefix = "/scratchbox/etc/scratchbox-version",
		 replace_by = "/usr/share/scratchbox2/version",
		 readonly = true, virtual_path = true},

		--
		-- Some of the scripts that starts up fremantle
		-- GUI test existense of /scratchbox so we point
		-- it to sb1_compat_dir.
		--
		{dir = "/scratchbox", replace_by = sb1_compat_dir,
		 readonly = true, virtual_path = true},

		-- gdb wants to have access to our dynamic linker also.
		{path = "/usr/lib/libsb2/ld-2.5.so", use_orig_path = true,
		 readonly = true},

		--
		-- Following 3 rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{path = "/etc/resolv.conf", force_orig_path = true,
		 readonly = true},

		--
		{prefix = "/var/run", map_to = session_dir},

		-- 
		{prefix = "/tmp", replace_by = tmp_dir_dest},

		-- 
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = sb2_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		-- -----------------------------------------------
		{prefix = sbox_user_home_dir, use_orig_path = true},

		-- "user" is a special username, and should be mapped
		-- to target_root
		-- (but note that if the real user name is "user",
		-- our previous rule handled that and this rule won't be used)
		{prefix = "/home/user", map_to = target_root,
		 readonly = target_root_is_readonly},

		-- Other home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},
		-- -----------------------------------------------

		-- The default is to map everything to target_root,
		-- except that we don't map the directory tree where
		-- sb2 was started.
		{prefix = unmapped_workdir, use_orig_path = true},

		{path = "/", chain = rootdir_rules},
		{prefix = "/", map_to = target_root,
		 readonly = target_root_is_readonly}
	}
}

--
-- Special case for dpkg: we don't want to use wrapped
-- launcher scripts here because we might be installing
-- (over) them.
--
local dpkg_chain = {
	next_chain = mapall_chain,
	binary = "dpkg",
	rules = {
		{ path = "/usr/bin/scratchbox-launcher.sh",
		    map_to = target_root,
		    readonly = target_root_is_readonly },
		{ path = "/etc/osso-af-init/dbus-systembus.sh",
		    map_to = target_root,
		    readonly = target_root_is_readonly },
	},
}

-- Special case for /bin/pwd: Some versions don't use getcwd(),
-- but instead the use open() + fstat() + fchdir() + getdents()
-- in a loop, and that fails if "/" is mapped to target_root.
local pwd_chain = {
	next_chain = mapall_chain,
	binary = "pwd",
	rules = {
		{path = "/", use_orig_path = true},
	},
}

export_chains = {
	dpkg_chain,
	pwd_chain,
	mapall_chain
}

-- Exec policy rules.

default_exec_policy = {
	name = "Default",
	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}

-- For target binaries:
-- First, note that "foreign" binaries are easy to handle, no problem there.
-- But if CPU transparency method has not been set, then host CPU == target CPU:
-- we have "target's native" and "host's native" binaries, that would look 
-- identical (and valid!) to the kernel. But they need to use different 
-- loaders and dynamic libraries! The solution is that we use the location
-- (as determined by the mapping engine) to decide the execution policy.

emulate_mode_target_ld_so = nil		-- default = not needed
emulate_mode_target_ld_library_path_prefix = ""
emulate_mode_target_ld_library_path_suffix = nil

-- used if libsb2.so is not available in target_root:
emulate_mode_target_ld_library_path_suffix = nil

if (conf_target_sb2_installed) then
	--
	-- When libsb2 is installed to target we don't want to map
	-- the path where it is found.  For example gdb needs access
	-- to the library and dynamic linker.  So here we insert special
	-- rules on top of mapall_chain that prevents sb2 to map these
	-- paths.
	--
	if (conf_target_libsb2_dir ~= nil) then
		table.insert(mapall_chain.rules, 1,
		    { prefix = conf_target_libsb2_dir, use_orig_path = true,
		      readonly = true }) 
	end
	if (conf_target_ld_so ~= nil) then
		table.insert(mapall_chain.rules, 1,
		    { path = conf_target_ld_so, use_orig_path = true,
		      readonly = true }) 

		-- use dynamic libraries from target, 
		-- when executing native binaries!
		emulate_mode_target_ld_so = conf_target_ld_so
	end
	emulate_mode_target_ld_library_path_prefix = conf_target_ld_so_library_path
else
	emulate_mode_target_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	emulate_mode_target_ld_library_path_suffix =
		host_ld_library_path_suffix
end


local exec_policy_target = {
	name = "Rootstrap",
	native_app_ld_so = emulate_mode_target_ld_so,
	native_app_ld_so_supports_argv0 = conf_target_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_target_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = target_root,
	native_app_ld_so_supports_nodefaultdirs = conf_target_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_target_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_target_ld_library_path_suffix,

	native_app_locale_path = conf_target_locale_path,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}

--
-- For tools: If tools_root is set and libsb2 has been installed there,
-- then dynamic libraries can be used from tools_root (otherwise we'll
-- be using the libs from the host OS)

tools = tools_root
if (not tools) then
	tools = "/"
end

local emulate_mode_tools_ld_so = nil		-- default = not needed
local emulate_mode_tools_ld_library_path_prefix = ""
local emulate_mode_tools_ld_library_path_suffix = ""

if ((tools_root ~= nil) and conf_tools_sb2_installed) then
	if (conf_tools_ld_so ~= nil) then
		-- Ok to use dynamic libraries from tools!
		emulate_mode_tools_ld_so = conf_tools_ld_so
	end
	emulate_mode_tools_ld_library_path_prefix = conf_tools_ld_so_library_path
	if (conf_tools_locale_path ~= nil) then
		-- use locales from tools
		devel_mode_locale_path = conf_tools_locale_path
	end
else
	emulate_mode_tools_ld_library_path_prefix =
		host_ld_library_path_libfakeroot ..
		host_ld_library_path_prefix ..
		host_ld_library_path_libsb2
	emulate_mode_tools_ld_library_path_suffix =
		host_ld_library_path_suffix
end

local exec_policy_tools = {
	name = "Tools",
	native_app_ld_so = emulate_mode_tools_ld_so,
	native_app_ld_so_supports_argv0 = conf_tools_ld_so_supports_argv0,
	native_app_ld_so_supports_rpath_prefix = conf_tools_ld_so_supports_rpath_prefix,
	native_app_ld_so_rpath_prefix = tools,
	native_app_ld_so_supports_nodefaultdirs = conf_tools_ld_so_supports_nodefaultdirs,

	native_app_ld_library_path_prefix = emulate_mode_tools_ld_library_path_prefix,
	native_app_ld_library_path_suffix = emulate_mode_tools_ld_library_path_suffix,

	native_app_ld_preload_prefix = host_ld_preload..fakeroot_ld_preload,
}


-- Note that the real path (mapped path) is used when looking up rules!
all_exec_policies_chain = {
	next_chain = nil,
	binary = nil,
	rules = {
		-- Tools. at least qemu might be used from there.
		{prefix = tools, exec_policy = exec_policy_tools},

                -- the home directory is expected to contain target binaries:
                {prefix = sbox_user_home_dir, exec_policy = exec_policy_target},

		-- Target binaries:
		{prefix = target_root, exec_policy = exec_policy_target},

		-- the place where the session was created is expected
		-- to contain target binaries:
		{prefix = sbox_workdir, exec_policy = exec_policy_target},

		-- DEFAULT RULE (must exist):
		{prefix = "/", exec_policy = default_exec_policy}
	}
}

exec_policy_chains = {
	all_exec_policies_chain
}

-- This table lists all exec policies - this is used when the current
-- process wants to locate the currently active policy
all_exec_policies = {
	exec_policy_target,
	exec_policy_tools,
	default_exec_policy,
}

