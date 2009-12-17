/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef __SB2_H
#define __SB2_H

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/* WARNING!!
 * pthread functions MUST NOT be used directly in the preload library.
 * see the warning in luaif/luaif.c, also see the examples in that
 * file (how to detect if pthread library is available, etc)
*/
#include <pthread.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

struct lua_instance {
	lua_State *lua;
	int mapping_disabled;
	int lua_instance_in_use; /* used only if debug messages are active */

	/* for path mapping logic: */
	char *host_cwd;
	char *virtual_reversed_cwd;
};

/* This version string is used to check that the lua scripts offer 
 * what the C files expect, and v.v.
 * Increment the serial number (first number) and update the initials
 * and date whenever the interface beween Lua and C is changed.
 *
 * * Differences between "28,lta-2008-09-23" and "35,lta-2008-10-01":
 *   - sbox_get_mapping_requirements(): parameter work_dir was removed
 *   - sbox_translate_path(): as above
 * * Differences between "35,lta-2008-10-01" and "53,lta-2008-11-10"
 *   - added new functions sb.get_forced_mapmode() and sb.get_session_perm()
 * * Differences between "59,lta-2008-12-04" and "53,lta-2008-11-10"
 *   - part of rule selection logic is now implemented in C.
 * * Differences between "60,2008-12-07" and "59,lta-2008-12-04"
 *   - Added special handler for /proc => sb.procfs_mapping_request() was
 *     added to luaif.c (and mapping.lua needs it)
 *   - sbox_get_mapping_requirements() now returns four values
 * * Differences between "61" and "60,2008-12-07"
 *   - added execve_map_script_interpreter()
 * * Differences between "62" and "61"
 *   - added sb.test_if_listed_in_envvar()
 * * Differences between "63" and "62"
 *   - sb_find_exec_policy() has been removed.
 * * Differences between "65" and "63"
 *   - The Lua side of the mapping engine now returns "flags" (bitmask) 
 *     to the C code; functions used to return separate booleans.
 *   - (Additionally, due to a previous bugfix in path_exists(), forcing 
 *     the library to be upgraded is a good thing to do in any case!)
 * * Differences between "66" and "65"
 *   - LD_PRELOAD and LD_LIBRARY_PATH environment variables
 *     must be set by the exec postprocessing code (in argvenvp.lua);
 *     sb_exec.c refuses to exec the program if these two are not set.
 *
 * NOTE: the corresponding identifier for Lua is in lua_scripts/main.lua
*/
#define SB2_LUA_C_INTERFACE_VERSION "66"

extern struct lua_instance *get_lua(void);
extern void release_lua(struct lua_instance *ptr);

#if 0
extern char *sb_decolonize_path(const char *path);
#endif

extern int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

extern int do_exec(const char *exec_fn_name, const char *file,
		char *const *argv, char *const *envp);

extern time_t get_sb2_timestamp(void);

extern char *procfs_mapping_request(char *path);

extern void dump_lua_stack(const char *msg, lua_State *L);

/* ------ debug/trace logging system for sb2: */
#define SB_LOGLEVEL_uninitialized (-1)
#define SB_LOGLEVEL_NONE	0
#define SB_LOGLEVEL_ERROR	1
#define SB_LOGLEVEL_WARNING	2
#define SB_LOGLEVEL_NOTICE	3
#define SB_LOGLEVEL_INFO	5
#define SB_LOGLEVEL_DEBUG	8
#define SB_LOGLEVEL_NOISE	9
#define SB_LOGLEVEL_NOISE2	10
#define SB_LOGLEVEL_NOISE3	11

extern void sblog_init(void);
extern void sblog_vprintf_line_to_logfile(const char *file, int line,
	int level, const char *format, va_list ap);
extern void sblog_printf_line_to_logfile(const char *file, int line,
	int level, const char *format,...);

extern int sb_loglevel__; /* do not access directly */

#define SB_LOG_INITIALIZED() (sb_loglevel__ >= SB_LOGLEVEL_NONE)

#define SB_LOG_IS_ACTIVE(level) ((level) <= sb_loglevel__)

#define SB_LOG(level, ...) \
	do { \
		if (SB_LOG_IS_ACTIVE(level)) { \
			sblog_printf_line_to_logfile( \
				__FILE__, __LINE__, level, __VA_ARGS__); \
		} \
	} while (0)

#define LIBSB2 "libsb2.so.1"

extern int sb2_global_vars_initialized__;
extern void sb2_initialize_global_variables(void);
extern char *sbox_session_dir;
extern char *sbox_session_mode;
extern char *sbox_session_perm;
extern char *sbox_orig_ld_preload;
extern char *sbox_orig_ld_library_path;
extern char *sbox_binary_name;
extern char *sbox_real_binary_name;
extern char *sbox_orig_binary_name;
extern char *sbox_active_exec_policy_name;

extern int pthread_library_is_available; /* flag */
extern pthread_t (*pthread_self_fnptr)(void);
extern int (*pthread_mutex_lock_fnptr)(pthread_mutex_t *mutex);
extern int (*pthread_mutex_unlock_fnptr)(pthread_mutex_t *mutex);

#endif
