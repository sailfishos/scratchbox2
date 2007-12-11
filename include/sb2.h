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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

struct lua_instance {
	lua_State *lua;
	char *script_dir;
	char *main_lua_script;
	int mapping_disabled;
};

void sb2_lua_init(void);
struct lua_instance *get_lua(void);
char *sb_decolonize_path(const char *path);

int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

int do_exec(const char *exec_fn_name, const char *file,
		char *const *argv, char *const *envp);

int ld_so_run_app(char *file, char **argv, char *const *envp);
int run_hashbang(const char *file, char *const *argv, char *const *envp);
int run_app(const char *file, char *const *argv, char *const *envp);
int run_cputransparency(const char *file, const char *unmapped_file,
			char *const *argv, char *const *envp);

int run_sbrsh(const char *sbrsh_bin, const char *target_root, const char *file,
              char *const *argv, char *const *envp);
int run_qemu(const char *qemu_bin, const char *file, char *const *argv,
		char *const *envp);

time_t get_sb2_timestamp(void);

/* ------ debug/trace logging system for sb2: */
#define SB_LOGLEVEL_uninitialized (-1)
#define SB_LOGLEVEL_NONE	0
#define SB_LOGLEVEL_ERROR	1
#define SB_LOGLEVEL_WARNING	2
#define SB_LOGLEVEL_INFO	5
#define SB_LOGLEVEL_DEBUG	8
#define SB_LOGLEVEL_NOISE	9

extern void sblog_init(void);
extern void sblog_vprintf_line_to_logfile(const char *file, int line,
	const char *format, va_list ap);
extern void sblog_printf_line_to_logfile(const char *file, int line,
	const char *format,...);

extern int sb_loglevel__; /* do not access directly */

#define SB_LOG(level, ...) \
	do { \
		if ((level) <= sb_loglevel__) { \
			sblog_printf_line_to_logfile( \
				__FILE__, __LINE__, __VA_ARGS__); \
		} \
	} while (0)

#define LIBSB2 "libsb2.so.1"

#endif
