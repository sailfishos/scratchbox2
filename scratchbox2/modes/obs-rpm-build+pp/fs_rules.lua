-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2011 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "105"
----------------------------------

-- use "==" to test options as long as there is only one possible option,
-- string.match() is slow..
if sbox_mode_specific_options == "use-global-tmp" then
	tmp_dir_dest = "/tmp"
	var_tmp_dir_dest = "/var/tmp"
else
	tmp_dir_dest = session_dir .. "/tmp"
	var_tmp_dir_dest = session_dir .. "/var/tmp"
end

test_first_target_then_tools_default_is_target = {
	{ if_exists_then_map_to = target_root, readonly = true },
	{ if_exists_then_map_to = tools, readonly = true },
	{ map_to = target_root, readonly = true }
}

test_first_target_then_host_default_is_target = {
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ if_exists_then_map_to = "/", protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always }
}

test_first_usr_bin_default_is_bin__replace = {
	{ if_exists_then_replace_by = target_root.."/usr/bin", protection = readonly_fs_always },
	{ replace_by = target_root.."/bin", protection = readonly_fs_always }
}

test_first_tools_then_target_default_is_tools = {
	{ if_exists_then_map_to = tools, protection = readonly_fs_always },
	{ if_exists_then_map_to = target_root, protection = readonly_fs_always },
	{ map_to = tools, protection = readonly_fs_always }
}

-- /usr/bin/perl, /usr/bin/python and related tools should
-- be mapped to target_root, but there are two exceptions:
-- 1) perl and python scripts from tools_root need to get
--    the interpreter from tools_root, too
-- 2) if the program can not be found from target_root,
--    but is present in tools_root, it will be used from
--    tools. This produces a warning, because it isn't always
--    safe to do so.

perl_lib_test = {
	{ if_active_exec_policy_is = "Tools-perl",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Target",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

perl_bin_test = {
	{ if_redirect_ignore_is_active = "/usr/bin/perl",
	  map_to = target_root, readonly = true },
	{ if_redirect_force_is_active = "/usr/bin/perl",
	  map_to = tools, readonly = true,
	  exec_policy_name = "Tools-perl" },
	{ if_active_exec_policy_is = "Target",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Tools-perl",
	  map_to = tools, readonly = true },
	{ if_exists_then_map_to = target_root, readonly = true },
	{ if_exists_then_map_to = tools_root,
	  log_level = "warning", log_message = "Mapped to tools_root",
	  readonly = true },
	{ map_to = target_root, readonly = true }
}

python_bin_test = {
	{ if_redirect_ignore_is_active = "/usr/bin/python",
	  map_to = target_root, readonly = true },
	{ if_redirect_force_is_active = "/usr/bin/python",
	  map_to = tools, readonly = true,
	  exec_policy_name = "Tools-python" },
	{ if_active_exec_policy_is = "Target",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Tools-python",
	  map_to = tools, readonly = true },
	{ map_to = target_root, readonly = true }
}

python_lib_test = {
	{ if_active_exec_policy_is = "Tools-python",
	  map_to = tools, readonly = true },
	{ if_active_exec_policy_is = "Target",
	  map_to = target_root, readonly = true },
	{ if_active_exec_policy_is = "Host",
	  use_orig_path = true, readonly = true },
	{ map_to = target_root, readonly = true }
}

-- accelerated programs:
-- Use a binary from tools_root, if it is availabe there.
-- Fallback to target_root, if it doesn't exist in tools.
accelerated_program_actions = {
	{ if_exists_then_map_to = tools, protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always },
}

-- conditionally accelerated programs:
-- check if file exists in target_root and only then try to accelerate it
conditionally_accelerated_program_actions = {
	{ if_exists_in = target_root, then_actions = accelerated_program_actions,
	  protection = readonly_fs_always },
	{ map_to = target_root, protection = readonly_fs_always },
}

-- Path == "/":
rootdir_rules = {
		-- Special case for /bin/pwd: Some versions don't use getcwd(),
		-- but instead the use open() + fstat() + fchdir() + getdents()
		-- in a loop, and that fails if "/" is mapped to target_root.
		{path = "/", binary_name = "pwd", use_orig_path = true},

		-- All other programs:
		{path = "/",
		    func_class = FUNC_CLASS_STAT + FUNC_CLASS_OPEN + FUNC_CLASS_SET_TIMES,
                    map_to = target_root, protection = readonly_fs_if_not_root },

		-- Default: Map to real root.
		{path = "/", use_orig_path = true},
}


emulate_mode_rules_bin = {
		{path="/bin/awk",
		 actions=accelerated_program_actions},
		{path="/bin/basename",
		 actions=accelerated_program_actions},
		{path="/bin/bash",
		 actions=accelerated_program_actions},
		{path="/bin/cat",
		 actions=accelerated_program_actions},
		{path="/bin/chgrp",
		 actions=accelerated_program_actions},
		{path="/bin/chmod",
		 actions=accelerated_program_actions},
		{path="/bin/chown",
		 actions=accelerated_program_actions},
		{path="/bin/cp",
		 actions=accelerated_program_actions},
		{path="/bin/cpio",
		 actions=accelerated_program_actions},
		{path="/bin/cut",
		 actions=accelerated_program_actions},
		{path="/bin/date",
		 actions=accelerated_program_actions},
		{path="/bin/dd",
		 actions=accelerated_program_actions},
		{path="/bin/echo",
		 actions=accelerated_program_actions},
		{path="/bin/egrep",
		 actions=accelerated_program_actions},
		{path="/bin/false",
		 actions=accelerated_program_actions},
		{path="/bin/fgrep",
		 actions=accelerated_program_actions},
		{path="/bin/fgrep",
		 actions=accelerated_program_actions},
		{path="/bin/find",
		 actions=accelerated_program_actions},
		{path="/bin/gawk",
		 actions=accelerated_program_actions},
		{path="/bin/grep",
		 actions=accelerated_program_actions},
		{path="/bin/gzip",
		 actions=accelerated_program_actions},
		{path="/bin/hostname",
		 actions=accelerated_program_actions},
		{path="/bin/link",
		 actions=accelerated_program_actions},
		{path="/bin/ln",
		 actions=accelerated_program_actions},
		{path="/bin/ls",
		 actions=accelerated_program_actions},
		{path="/bin/mkdir",
		 actions=accelerated_program_actions},
		{path="/bin/mknod",
		 actions=accelerated_program_actions},
		{path="/bin/mktemp",
		 actions=accelerated_program_actions},
		{path="/bin/mv",
		 actions=accelerated_program_actions},
		{path="/bin/rm",
		 actions=accelerated_program_actions},
		{path="/bin/rmdir",
		 actions=accelerated_program_actions},
		{path="/bin/sed",
		 actions=accelerated_program_actions},
		{path="/bin/sh",
		 actions=accelerated_program_actions},
		{path="/bin/sleep",
		 actions=accelerated_program_actions},
		{path="/bin/sort",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tar",
		 actions=accelerated_program_actions},
		{path="/bin/touch",
		 actions=accelerated_program_actions},
		{path="/bin/true",
		 actions=accelerated_program_actions},
		{path="/bin/pwd",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode .. "/pwd",
		 protection_readonly_fs_always},

		-- rpm rules
		{path = "/bin/rpm",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		-- end of rpm rules
		
		{name = "/bin default rule", dir = "/bin", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_usr_bin = {
		{path="/usr/bin/awk",
		 actions=accelerated_program_actions},
		{path="/usr/bin/base64",
		 actions=accelerated_program_actions},
		{path="/usr/bin/basename",
		 actions=accelerated_program_actions},
		{path="/usr/bin/bison",
		 actions=accelerated_program_actions},
		{path="/usr/bin/bzip2",
		 actions=accelerated_program_actions},
		{path="/usr/bin/cksum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/cmp",
		 actions=accelerated_program_actions},
		{path="/usr/bin/comm",
		 actions=accelerated_program_actions},
		{path="/usr/bin/csplit",
		 actions=accelerated_program_actions},
		{path="/usr/bin/diff",
		 actions=accelerated_program_actions},
		{path="/usr/bin/diff3",
		 actions=accelerated_program_actions},
		{path="/usr/bin/diff3",
		 actions=accelerated_program_actions},
		{path="/usr/bin/dir",
		 actions=accelerated_program_actions},
		{path="/usr/bin/dircolors",
		 actions=accelerated_program_actions},
		{path="/usr/bin/dirname",
		 actions=accelerated_program_actions},
		{path="/usr/bin/du",
		 actions=accelerated_program_actions},
		{path="/usr/bin/egrep",
		 actions=accelerated_program_actions},
		{path="/usr/bin/env",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-addr2line",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-ar",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-elfcmp",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-elflint",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-findtextrel",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-make-debug-archive",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-nm",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-objdump",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-ranlib",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-readelf",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-size",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-strings",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-strip",
		 actions=accelerated_program_actions},
		{path="/usr/bin/eu-unstrip",
		 actions=accelerated_program_actions},
		{path="/usr/bin/expand",
		 actions=accelerated_program_actions},
		{path="/usr/bin/expr",
		 actions=accelerated_program_actions},
		{path="/usr/bin/factor",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fdupes",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fgrep",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fdupes",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fgrep",
		 actions=accelerated_program_actions},
		{path="/usr/bin/file",
		 actions=accelerated_program_actions},
		{path="/usr/bin/find",
		 actions=accelerated_program_actions},
		{path="/usr/bin/flex",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fmt",
		 actions=accelerated_program_actions},
		{path="/usr/bin/fold",
		 actions=accelerated_program_actions},
		{path="/usr/bin/gawk",
		 actions=accelerated_program_actions},
		{path="/usr/bin/grep",
		 actions=accelerated_program_actions},
		{path="/usr/bin/groff",
		 actions=accelerated_program_actions},
		{path="/usr/bin/grotty",
		 actions=accelerated_program_actions},
		{path="/usr/bin/groff",
		 actions=accelerated_program_actions},
		{path="/usr/bin/grotty",
		 actions=accelerated_program_actions},
		{path="/usr/bin/gtbl",
		 actions=accelerated_program_actions},
		{path="/usr/bin/gzip",
		 actions=accelerated_program_actions},
		{path="/usr/bin/head",
		 actions=accelerated_program_actions},
		{path="/usr/bin/hexdump",
		 actions=accelerated_program_actions},
		{path="/usr/bin/hostid",
		 actions=accelerated_program_actions},
		{path="/usr/bin/id",
		 actions=accelerated_program_actions},
		{path="/usr/bin/install",
		 actions=accelerated_program_actions},
		{path="/usr/bin/join",
		 actions=accelerated_program_actions},
		{path="/usr/bin/llc",
		 actions=accelerated_program_actions},
		{path="/usr/bin/logname",
		 actions=accelerated_program_actions},
		{path="/usr/bin/m4",
		 actions=accelerated_program_actions},
		{path="/usr/bin/make",
		 actions=accelerated_program_actions},
		{path="/usr/bin/md5sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/mkfifo",
		 actions=accelerated_program_actions},
		{path="/usr/bin/nl",
		 actions=accelerated_program_actions},
		{path="/usr/bin/nohup",
		 actions=accelerated_program_actions},
		{path="/usr/bin/od",
		 actions=accelerated_program_actions},
		{path="/usr/bin/paste",
		 actions=accelerated_program_actions},
		{path="/usr/bin/patch",
		 actions=accelerated_program_actions},
		{path="/usr/bin/pathchk",
		 actions=accelerated_program_actions},
		{path="/usr/bin/pinky",
		 actions=accelerated_program_actions},
		{path="/usr/bin/pr",
		 actions=accelerated_program_actions},
		{path="/usr/bin/printf",
		 actions=accelerated_program_actions},
		{path="/usr/bin/ptx",
		 actions=accelerated_program_actions},
		{path="/usr/bin/readlink",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sed",
		 actions=accelerated_program_actions},
		{path="/usr/bin/seq",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sha1sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sha224sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sha256sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sha384sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sha512sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/shred",
		 actions=accelerated_program_actions},
		{path="/usr/bin/shuf",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sort",
		 actions=accelerated_program_actions},
		{path="/usr/bin/split",
		 actions=accelerated_program_actions},
		{path="/usr/bin/stat",
		 actions=accelerated_program_actions},
		{path="/usr/bin/sum",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tac",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tail",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tee",
		 actions=accelerated_program_actions},
		{path="/usr/bin/test",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tr",
		 actions=accelerated_program_actions},
		{path="/usr/bin/troff",
		 actions=accelerated_program_actions},
		{path="/usr/bin/troff",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tsort",
		 actions=accelerated_program_actions},
		{path="/usr/bin/tty",
		 actions=accelerated_program_actions},
		{path="/usr/bin/unexpand",
		 actions=accelerated_program_actions},
		{path="/usr/bin/uniq",
		 actions=accelerated_program_actions},
		{path="/usr/bin/users",
		 actions=accelerated_program_actions},
		{path="/usr/bin/vdir",
		 actions=accelerated_program_actions},
		{path="/usr/bin/wc",
		 actions=accelerated_program_actions},
		{path="/usr/bin/who",
		 actions=accelerated_program_actions},
		{path="/usr/bin/whoami",
		 actions=accelerated_program_actions},
		{path="/usr/bin/xargs",
		 actions=accelerated_program_actions},
		{path="/usr/bin/yes",
		 actions=accelerated_program_actions},

		{path="/usr/bin/localedef",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/ccache",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/qtchooser",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/cpio",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/zip",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/xz",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
		{path="/usr/bin/doxygen",
		 func_class = FUNC_CLASS_EXEC,
		 actions=conditionally_accelerated_program_actions},
		{path="/usr/bin/cmake",
		 func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},

		-- perl & python:
		-- 	processing depends on SBOX_REDIRECT_IGNORE,
		--	SBOX_REDIRECT_FORCE and 
		--	name of the current exec policy. 
		--	(these are real prefixes, version number may
		--	be included in the name (/usr/bin/python2.5 etc))
		{prefix = "/usr/bin/perl", actions = perl_bin_test},
		{prefix = "/usr/bin/python", actions = python_bin_test},

		{path = "/usr/bin/sb2-show", use_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/usr/bin/sb2-qemu-gdbserver-prepare",
		    use_orig_path = true, protection = readonly_fs_always},
		{path = "/usr/bin/sb2-session", use_orig_path = true,
		 protection = readonly_fs_always},

		-- next, automatically generated rules for /usr/bin:
		{name = "/usr/bin autorules", dir = "/usr/bin", rules = argvmods_rules_for_usr_bin,
		 virtual_path = true}, -- don't reverse these.

		-- rpm rules
		{prefix = "/usr/bin/rpm",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/zypper",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/deltainfoxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/dumpsolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/installcheck",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/mergesolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/repomdxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/rpmdb2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/rpmmd2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/rpms2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/testsolv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		{path = "/usr/bin/updateinfoxml2solv",
		 func_class = FUNC_CLASS_EXEC,
		 actions = accelerated_program_actions},
		-- rpm scripts
		{path = "/usr/bin/rpmlint",
		 actions = accelerated_program_actions},
		-- end of rpm rules

		-- some "famous" scripts:
		-- (these may have embedded version numbers in
		-- the names => uses prefix rules for matching)
		{prefix = "/usr/bin/aclocal",
		 actions = accelerated_program_actions},
		{prefix = "/usr/bin/autoconf",
		 actions = accelerated_program_actions},
		{prefix = "/usr/bin/autoheader",
		 actions = accelerated_program_actions},
		{prefix = "/usr/bin/autom4te",
		 actions = accelerated_program_actions},
		{prefix = "/usr/bin/automake",
		 actions = accelerated_program_actions},
		{prefix = "/usr/bin/autoreconf",
		 actions = accelerated_program_actions},

		--
		{name = "/usr/bin default rule", dir = "/usr/bin", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

usr_share_rules = {
		-- -----------------------------------------------
		-- 1. General SB2 environment:

		{prefix = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- Perl:
		{prefix = "/usr/share/perl", actions = perl_lib_test},

		-- Python:
		{prefix = "/usr/share/python", actions = python_lib_test},
		{prefix = "/usr/share/pygobject", actions = python_lib_test},

		-- -----------------------------------------------
		
		{path = "/usr/share/aclocal",
			union_dir = {
				target_root.."/usr/share/aclocal",
				tools.."/usr/share/aclocal",
			},
			readonly = true,
		},

		{path = "/usr/share/pkgconfig",
			union_dir = {
				target_root.."/usr/share/pkgconfig",
				tools.."/usr/share/pkgconfig",
			},
			readonly = true,
		},

		-- 100. DEFAULT RULES:
		{dir = "/usr/share",
		 actions = test_first_target_then_tools_default_is_target},
}

emulate_mode_rules_usr = {
		{name = "/usr/bin branch", dir = "/usr/bin", rules = emulate_mode_rules_usr_bin},
                {path = "/usr/lib/rpm/elfdeps", func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
                {path = "/usr/lib/rpm/debugedit", func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
                {path = "/usr/lib/rpm/javadeps", func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
                {path = "/usr/lib/rpm/rpmdeps", func_class = FUNC_CLASS_EXEC,
		 actions=accelerated_program_actions},
                {path = "/usr/lib/qt4/bin/qmake", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt4/bin/moc", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt4/bin/uic", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt5/bin/qmake", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt5/bin/moc", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt5/bin/uic", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/lib/qt5/bin/qdoc", func_class = FUNC_CLASS_EXEC,
                 actions=accelerated_program_actions},
                {path = "/usr/include/python2.7/pyconfig.h",
                 actions=python_lib_test},

		-- gdb wants to have access to our dynamic linker also,
		-- /usr/lib/libsb2/wrappers/*, etc.
		{dir = "/usr/lib/libsb2", use_orig_path = true,
		 protection = readonly_fs_always},

		{dir = "/usr/lib/gcc", actions = test_first_tools_then_target_default_is_tools},

		{prefix = "/usr/lib/perl", actions = perl_lib_test},

		{prefix = "/usr/lib/python", actions = python_lib_test},

		{dir = "/usr/share", rules = usr_share_rules},

		-- If a program from tools loads plugins,
		-- they should be dlopened from tools as well.
		-- However, libdir in tools can be different than one in target.
		{dir = "/usr/lib", func_class = FUNC_CLASS_DLOPEN,
		 actions = {
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_replace_by = tools .. "/usr/lib64",
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_map_to = tools,
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Host",
		   if_exists_then_replace_by = tools .. "/usr/lib64",
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Host",
		   if_exists_then_map_to = tools,
		   protection = readonly_fs_always},
		  { map_to = target_root, protection = readonly_fs_always },
		 },
		},
		-- and if the program looks in lib64 and the emulator can't find
		-- it (because it's 32 bit) then look in /usr/lib...
		{dir = "/usr/lib64", func_class = FUNC_CLASS_DLOPEN,
		 actions = {
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_replace_by = tools .. "/usr/lib",
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Tools",
		   if_exists_then_map_to = tools,
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Host",
		   if_exists_then_replace_by = tools .. "/usr/lib",
		   protection = readonly_fs_always},
		  {if_active_exec_policy_is = "Host",
		   if_exists_then_map_to = tools,
		   protection = readonly_fs_always},
		  { map_to = target_root, protection = readonly_fs_always },
		 },
		},

		{dir = "/usr", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_etc = {
		-- Following rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 protection = readonly_fs_always},
		{path = "/etc/resolv.conf", force_orig_path = true,
		 protection = readonly_fs_always},

		-- Perl & Python:
		{prefix = "/etc/perl", actions = perl_lib_test},
		{prefix = "/etc/python", actions = python_lib_test},

		{dir = "/etc", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

emulate_mode_rules_var = {
		-- Following rule are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		protection = readonly_fs_always},

		--
		{dir = "/var/run", map_to = session_dir},
		{dir = "/var/tmp", replace_by = var_tmp_dir_dest},

		{dir = "/var", map_to = target_root,
		protection = readonly_fs_if_not_root}
}

emulate_mode_rules_home = {
		-- Default: Not mapped, R/W access.
		{dir = "/home", use_orig_path = true},
}

emulate_mode_rules_opt = {
		-- for rpmlint:
		{dir = "/opt/testing", 
		 actions = test_first_tools_then_target_default_is_tools},
		--

		{dir = "/opt", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

sys_rules = {
		{dir = "/sys", use_orig_path = true},
}


emulate_mode_rules = {
		-- First paths that should never be mapped:
		{dir = session_dir, use_orig_path = true},

		{path = sbox_cputransparency_cmd, use_orig_path = true,
		 protection = readonly_fs_always},

		{dir = sbox_target_toolchain_dir, use_orig_path = true,
		 virtual_path = true, -- don't try to reverse this
		  protection = readonly_fs_always},

		--{dir = target_root, use_orig_path = true,
		-- protection = readonly_fs_if_not_root},
		{dir = target_root, use_orig_path = true,
		 virtual_path = true, -- don't try to reverse this
		 -- protection = readonly_fs_if_not_root
		},

		{path = os.getenv("SSH_AUTH_SOCK"), use_orig_path = true},

		-- ldconfig is static binary, and needs to be wrapped
		-- Gdb needs some special parameters before it
		-- can be run so we wrap it.
		{dir = "/sb2/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 protection = readonly_fs_always},

		-- 
		{dir = "/tmp", replace_by = tmp_dir_dest},

		{dir = "/dev", rules = import_from_fs_rule_library("dev")},

		{dir = "/proc", rules = import_from_fs_rule_library("proc")},
		{dir = "/sys", rules = sys_rules},

		{dir = sbox_dir .. "/share/scratchbox2",
		 use_orig_path = true},

		-- The real sbox_dir.."/lib/libsb2" must be available:
		--
		-- When libsb2 is installed to target we don't want to map
		-- the path where it is found.  For example gdb needs access
		-- to the library and dynamic linker, and these may be in
		-- target_root, or under sbox_dir.."/lib/libsb2", or
		-- under ~/.scratchbox2.
		{dir = sbox_dir .. "/lib/libsb2",
		 actions = test_first_target_then_host_default_is_target},

		-- -----------------------------------------------
		-- home directories:
		{dir = "/home", rules = emulate_mode_rules_home},
		-- -----------------------------------------------

		{dir = "/usr", rules = emulate_mode_rules_usr},
		{dir = "/bin", rules = emulate_mode_rules_bin},
		{dir = "/etc", rules = emulate_mode_rules_etc},
		{dir = "/var", rules = emulate_mode_rules_var},
		{dir = "/opt", rules = emulate_mode_rules_opt},

		{path = "/", rules = rootdir_rules},
		{prefix = "/", map_to = target_root,
		 protection = readonly_fs_if_not_root}
}

-- This allows access to tools with full host paths,
-- this is needed for example to be able to
-- start CPU transparency from tools.
-- Used only when tools_root is set.
local tools_rules = {
		{dir = tools_root, use_orig_path = true},
		{prefix = "/", rules = emulate_mode_rules},
}

-- allow user to extend these rules with a ~/.sbrules file
import_from_fs_rule_library("user_rules")

-- Define /parentroot as being outside like /home, this is a Mer SDK
-- path convention
use_outside_path("/parentroot")

-- Now run ~/.sbrules
run_user_sbrules()

if (tools_root ~= nil) and (tools_root ~= "/") then
        -- Tools root is set.
	fs_mapping_rules = tools_rules
else
        -- No tools_root.
	fs_mapping_rules = emulate_mode_rules
end

