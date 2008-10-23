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

struct lua_instance {
	lua_State *lua;
	int mapping_disabled;
};

/* This version string is used to check that the lua scripts offer 
 * what the C files expect, and v.v.
 * Increment the serial number (first number) and update the initials
 * and date whenever the interface beween Lua and C is changed.
 *
 * * Differences between "28,lta-2008-09-23" and "35,lta-2008-10-01":
 *   - sbox_get_mapping_requirements(): parameter work_dir was removed
 *   - sbox_translate_path(): as above
 *
 * NOTE: the corresponding identifier for Lua is in lua_scripts/main.lua
*/
#define SB2_LUA_C_INTERFACE_VERSION "35,lta-2008-10-01"

struct lua_instance *get_lua(void);

#if 0
char *sb_decolonize_path(const char *path);
#endif

int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

int do_exec(const char *exec_fn_name, const char *file,
		char *const *argv, char *const *envp);

time_t get_sb2_timestamp(void);

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
extern char *sbox_orig_ld_preload;
extern char *sbox_orig_ld_library_path;

extern int pthread_library_is_available; /* flag */
extern pthread_t (*pthread_self_fnptr)(void);

#endif
