-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- License: LGPL-2.1

--
-- Constants which can be used from the rule files:
--

-- These must match the flag definitions in mapping.h:
RULE_FLAGS_READONLY = 1
RULE_FLAGS_CALL_TRANSLATE_FOR_ALL = 2
RULE_FLAGS_FORCE_ORIG_PATH = 4
RULE_FLAGS_READONLY_FS_IF_NOT_ROOT = 8
RULE_FLAGS_READONLY_FS_ALWAYS = 16
RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT = 32

-- Function class (bitmask) definitions for rule files:
-- These must match SB2_INTERFACE_CLASS_* definitions in
-- include/mapping.h
FUNC_CLASS_OPEN		= 0x1
FUNC_CLASS_STAT		= 0x2
FUNC_CLASS_EXEC		= 0x4
FUNC_CLASS_SOCKADDR	= 0x8
FUNC_CLASS_FTSOPEN	= 0x10
FUNC_CLASS_GLOB		= 0x20
FUNC_CLASS_GETCWD	= 0x40
FUNC_CLASS_REALPATH	= 0x80
FUNC_CLASS_SET_TIMES	= 0x100
FUNC_CLASS_L10N		= 0x200
FUNC_CLASS_MKNOD	= 0x400
FUNC_CLASS_RENAME	= 0x800
FUNC_CLASS_SYMLINK	= 0x2000
FUNC_CLASS_CREAT	= 0x4000
FUNC_CLASS_REMOVE	= 0x8000
FUNC_CLASS_CHROOT	= 0x10000
FUNC_CLASS_DLOPEN	= 0x80000

-- "protection" attribute:
readonly_fs_if_not_root = 1
readonly_fs_always = 2

