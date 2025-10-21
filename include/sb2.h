/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#if 0
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <spawn.h>

/* "sb2context" used to be called "lua_instance", but
 * was renamed because it is now used for other purposes,
 * too, and can even be used without activating Lua system
 * (i.e. the "lua" pointer may remain NULL. See
 * functions "get_sb2context() and get_sb2context_lua())
*/
struct sb2context {
#if 0
	/* Lua: */
	lua_State *lua;
#endif

	/* general: */
	int mapping_disabled;
	int sb2context_in_use; /* used only if debug messages are active */

	/* for path mapping logic: */
	char *host_cwd;
	char *virtual_reversed_cwd;
};

/* Library interface version string:
 *
 * Originally, This version string was used to check that the lua scripts offer
 * what the C files expect, and v.v.
 * Now, when lua is not anymore included in libsb2, it is only used
 * to identify the C interface (e.g. for sb2-show), and to locate
 * a matching preload library (see session setup code in the "sb2"
 * script)
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
 * * Differences between "67" and "66"
 *   - Introduced a wrapper for system() (no real changes to the
 *     interface, but this fixes a serious bug introduced by the
 *     previous change; this number is incremented to force an
 *     upgrade now!)
 * * Differences between "68" and "67"
 *   - added a wrapper for popen(). Added a new function to argvenvp.lua.
 * * Differences between "69" and "68"
 *   - minor changes to the popen() implementation.
 * * Differences between "70" and "69"
 *   - bug fixes in luaif/paths.c
 * * Differences between "71" and "70"
 *   - sockaddr_un can now handle "abstract" socket names (a linux extension)
 * * Differences between "72" and "71"
 *   - added wrapper for utimensat
 * * Differences between "74" and "72"
 *   - added many wrappers (__*_chk(), etc)
 * * Differences between "75" and "74"
 *   - added features for union directory simulation
 * * Differences between "77" and "75"
 *   - Mapping rule is not anymore relayed to the exec logic;
 *     arguments and return value lists of lua functions
 *     sb_execve_map_script_interpreter() and
 *     sb_execve_postprocess() were modified.
 * * Differences between "90" and "77"
 *   - Initial version for sb2, vrs 2.3.x
 * * New in version "91":
 *   - networking rules have been added, couple
 *     of new functions were introduced.
 * * New in version "92":
 *   - "ruletree" functions have been added.
 * * New in version "93":
 *   - added sb.get_session_dir()
 * * New in version "94":
 *   - new, simpler and better ruletree api (added catalog_set(),
 *     catalog_get(), new_string() and removed stuff which
 *     was derived from the old prototype)
 * * New in version "95":
 *   - "set_path" mapping action has been added to
 *     both Lua and C mapping engines.
 * * New in version "96":
 *   - uint32 and boolean types in ruletree + related interf.functions
 * * New in version "97":
 *   - "sblib.*" functions
 * * New in version "98":
 *   - Lua fucntion sbox_execve_preprocess() was removed
 *     exec preprocessing code is now implemented in C)
 * * New in 99: Moved exec policy selection rules to ruletree
 * * New in 120:
 *   - New function sb.test_fn_class_match()
 *   - All mapping functions take an additional parameter
 *     (func_class)
 * * New in 121:
 *   - "fakeroot" isn't supported anymore, instead
 *     SB2's own Vperm subsystem offers similar functionality
 * * 122:
 *     sb_execve_map_script_interpreter() was removed
 * * 123:
 *     sbox_get_host_policy_ld_params() was removed
 * * 124:
 *     sb_execve_postprocess_native_executable() was re-written
 *     in C => sb_execve_postprocess() must not be called with
 *     exec_type=="native" anymore
 * * 125:
 *     not directly related to Lua/C interface: Rule tree
 *     header was changed (vrs 6).
 * * 126:
 *     sbox_map_network_addr() and sb.test_net_addr_match()
 *     were removed.
 * * 127:
 *     exec postprocessing is completely implemented in C,
 *     sb_execve_postprocess() is not needed anymore.
 * * 128:
 *     new interface classes SYMLINK and CREAT
 * * 129:
 *     chroot() is now simulated, and information about that
 *     is passed in environment variable when the simulation is
 *     active + new interface class CHROOT
 * * 130:
 *     new flag for rules: force_orig_path_unless_chroot
*/
#define SB2_LUA_C_INTERFACE_VERSION "130"

/* This version string is used to check that init.lua offers
 * what sb2d expects, and v.v.
*/
#define SB2D_LUA_C_INTERFACE_VERSION "301"

/* get sb2context, without activating lua: */
extern struct sb2context *get_sb2context(void);
/* get sb2context, activate lua if not already done: */
extern struct sb2context *get_sb2context_lua(void);
extern void sb2context_initialize_lua(struct sb2context *sb2ctx);
extern void release_sb2context(struct sb2context *ptr);

#if 0
extern char *sb_decolonize_path(const char *path);
#endif

extern int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

extern int do_exec(int *result_errno_ptr, const char *exec_fn_name, const char *file,
		char *const *argv, char *const *envp);

extern int sb_next_posix_spawn(pid_t* pid, const char *path,
        const posix_spawn_file_actions_t *file_actions,
        const posix_spawnattr_t *attrp, char *const argv [],
        char *const envp[]);

extern int do_posix_spawn(int *result_errno_ptr,
        const char *exec_fn_name, pid_t *pid, const char *orig_path,
        const posix_spawn_file_actions_t *file_actions,
        const posix_spawnattr_t *attrp,
        char *const *orig_argv, char *const *orig_envp);

extern time_t get_sb2_timestamp(void);

extern char *procfs_mapping_request(const char *path);

#if 0
extern void dump_lua_stack(const char *msg, lua_State *L);
#endif

extern int sb_path_exists(const char *path);

/* ------ debug/trace logging system for sb2: */
#define SB_LOGLEVEL_uninitialized (-1)
#define SB_LOGLEVEL_NONE	0
#define SB_LOGLEVEL_ERROR	1
#define SB_LOGLEVEL_WARNING	2
#define SB_LOGLEVEL_NETWORK	3
#define SB_LOGLEVEL_NOTICE	4
#define SB_LOGLEVEL_INFO	5
#define SB_LOGLEVEL_DEBUG	8
#define SB_LOGLEVEL_NOISE	9
#define SB_LOGLEVEL_NOISE2	10
#define SB_LOGLEVEL_NOISE3	11

extern void sblog_init(void);
extern void sblog_init_level_logfile_format(const char *opt_level,
	const char *opt_logfile, const char *opt_format);
extern int sblog_level_name_to_number(const char *level_str);

extern void sblog_vprintf_line_to_logfile(const char *file, int line,
	int level, const char *format, va_list ap);
extern void sblog_printf_line_to_logfile(const char *file, int line,
	int level, const char *format,...) __attribute__ ((format(printf, 4, 5)));

extern int sb_loglevel__; /* do not access directly */
extern int sb_log_initial_pid__; /* current PID will be recorded here
				  * when the logger is initialized */

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
extern char *sbox_vperm_ids;
extern char *sbox_network_mode;
extern char *sbox_orig_ld_preload;
extern char *sbox_orig_ld_library_path;
extern char *sbox_binary_name;
extern char *sbox_exec_name;
extern char *sbox_real_binary_name;
extern char *sbox_orig_binary_name;
extern char *sbox_active_exec_policy_name;
extern char *sbox_mapping_method;

extern char *sbox_chroot_path; /* virtual path to the chroot directory. */

extern void check_pthread_library(void);

extern int pthread_library_is_available; /* flag */
extern pthread_t (*pthread_self_fnptr)(void);
extern int (*pthread_mutex_lock_fnptr)(pthread_mutex_t *mutex);
extern int (*pthread_mutex_unlock_fnptr)(pthread_mutex_t *mutex);

extern int pthread_detection_done;

extern int (*pthread_key_create_fnptr)(pthread_key_t *key,
	 void (*destructor)(void*));
extern void *(*pthread_getspecific_fnptr)(pthread_key_t key);
extern int (*pthread_setspecific_fnptr)(pthread_key_t key,
	const void *value);
extern int (*pthread_once_fnptr)(pthread_once_t *, void (*)(void));

#if 0
extern void lua_string_table_to_strvec(lua_State *l,
	int lua_stack_offs, char ***args, int new_argc);
void strvec_free(char **args);
#endif

#if 0
extern int lua_bind_sblib_functions(lua_State *l);
extern int lua_sb_log(lua_State *l);
extern int lua_sb_path_exists(lua_State *l);
extern int lua_sb_debug_messages_enabled(lua_State *l);
extern int lua_sb_isprefix(lua_State *l);
extern int lua_sb_test_path_match(lua_State *l);
extern int lua_sb_readlink(lua_State *l);
#endif


extern int test_if_str_in_colon_separated_list_from_env(
	const char *str, const char *env_var_name);

#endif
