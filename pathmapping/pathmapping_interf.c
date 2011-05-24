/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (symlink- and path resolution code refactored by Lauri T. Aarnio at Nokia)
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Pathmapping subsystem: Public interfaces (exported to other parts of SB2)
 *
*/

#if 0
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <string.h>
#include <libgen.h>
#define _GNU_SOURCE
#else
#include <string.h>
#include <libgen.h>
#endif

#include <limits.h>
#include <sys/param.h>
#include <sys/file.h>
#include <assert.h>
#include <pthread.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include <mapping.h>
#include <sb2.h>
#include "libsb2.h"
#include "exported.h"

#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif

#include "pathmapping.h" /* get private definitions of this subsystem */

#define MAPPING_METHOD_NOT_SET	 	0
#define MAPPING_METHOD_C_ENGINE	 	01
#define MAPPING_METHOD_LUA_ENGINE	02
#define MAPPING_METHOD_BOTH_ENGINES	(MAPPING_METHOD_C_ENGINE|MAPPING_METHOD_LUA_ENGINE)

static int mapping_method = MAPPING_METHOD_NOT_SET;

static void check_mapping_method(void)
{
	if (mapping_method == MAPPING_METHOD_NOT_SET) {
		const char *mp;
		mp = getenv("SBOX_MAPPING_METHOD");

		if (mp) {
			if (strchr(mp,'c')|| strchr(mp,'C')) {
				mapping_method |= MAPPING_METHOD_C_ENGINE;
				SB_LOG(SB_LOGLEVEL_INFO,
					"Activated 'C' mapping method");
			}
			if (strchr(mp,'l')|| strchr(mp,'L')) {
				mapping_method |= MAPPING_METHOD_LUA_ENGINE;
				SB_LOG(SB_LOGLEVEL_INFO,
					"Activated 'Lua' mapping method");
			}
			if (mapping_method == MAPPING_METHOD_NOT_SET) {
				/* default to both */
				mapping_method = MAPPING_METHOD_BOTH_ENGINES;
				SB_LOG(SB_LOGLEVEL_INFO,
					"Error in SBOX_MAPPING_METHOD variable, using both mapping methods");
			}
		} else {
			/* default to both */
			mapping_method = MAPPING_METHOD_BOTH_ENGINES;
			SB_LOG(SB_LOGLEVEL_INFO,
				"Activated both mapping methods");
		}
	}
}

static void compare_results_from_c_and_lua_engines(
	const char *fn_name,
	const char *c_res,
	const char *lua_res)
{
	if (c_res && lua_res) {
		if (!strcmp(c_res, lua_res)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: ResultCheck: same, OK",
				fn_name);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: ResultCheck: DIFFERENT: C='%s', Lua='%s'",
				fn_name, c_res, lua_res);
		}
	} else {
		if (!c_res) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: ResultCheck: no result from C (Lua='%s')",
				fn_name, lua_res);
		}
		if (!lua_res) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: ResultCheck: no result from Lua (C='%s')",
				fn_name, c_res);
		}
	}
}

/* ========== Public interfaces to the mapping & resolution code: ========== */

void fwd_map_path(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	int exec_mode,
	mapping_results_t *res)
{
	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		mapping_results_t res2;

		check_mapping_method();

		switch (mapping_method) {
		case MAPPING_METHOD_C_ENGINE:
			if (sbox_map_path_internal__c_engine(binary_name,
				func_name, virtual_path,
				dont_resolve_final_symlink, 0, res) < 0) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"C path mapping engine failed, fallback to Lua (%s)",
					virtual_path);
				sbox_map_path_internal__lua_engine(binary_name,
					func_name, virtual_path,
					dont_resolve_final_symlink, 0, res);
			}
			return;
		case MAPPING_METHOD_LUA_ENGINE:
			sbox_map_path_internal__lua_engine(binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, res);
			return;
		case MAPPING_METHOD_BOTH_ENGINES:
			clear_mapping_results_struct(&res2);
			sbox_map_path_internal__lua_engine(binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, res);
			if (sbox_map_path_internal__c_engine(binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, &res2) < 0) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"C path mapping engine failed, can't compare (%s)",
					virtual_path);
			} else {
				compare_results_from_c_and_lua_engines(
					__func__, res2.mres_result_path,
					res->mres_result_path);
			}
			free_mapping_results(&res2);
			return;
		default:
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Invalid mapping method",
				__func__);
		}
	}
}

void sbox_map_path_for_sb2show(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	fwd_map_path(binary_name, func_name, virtual_path,
		0/*dont_resolve_final_symlink*/, 0/*exec_mode*/, res);
}

void sbox_map_path(
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	mapping_results_t *res)
{
	fwd_map_path(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
		func_name, virtual_path,
		dont_resolve_final_symlink, 0/*exec_mode*/, res);
}


void sbox_map_path_at(
	const char *func_name,
	int dirfd,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	mapping_results_t *res)
{
	const char *dirfd_path;

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
		return;
	}

	if ((*virtual_path == '/')
#ifdef AT_FDCWD
	    || (dirfd == AT_FDCWD)
#endif
	   ) {
		/* same as sbox_map_path() */
		fwd_map_path(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name, virtual_path,
			dont_resolve_final_symlink, 0/*exec_mode*/, res);
		return;
	}

	/* relative to something else than CWD */
	dirfd_path = fdpathdb_find_path(dirfd);

	if (dirfd_path) {
		/* pathname found */
		char *virtual_abs_path_at_fd = NULL;

		if (asprintf(&virtual_abs_path_at_fd, "%s/%s", dirfd_path, virtual_path) < 0) {
			/* asprintf failed */
			abort();
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"Synthetic path for %s(%d,'%s') => '%s'",
			func_name, dirfd, virtual_path, virtual_abs_path_at_fd);

		fwd_map_path(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			virtual_abs_path_at_fd, dont_resolve_final_symlink, 0/*exec_mode*/, res);
		free(virtual_abs_path_at_fd);

		return;
	}

	/* name not found. Can't do much here, log a warning and return
	 * the original relative path. That will work if we are lucky, but
	 * not always..  */
	SB_LOG(SB_LOGLEVEL_WARNING, "Path not found for FD %d, for %s(%s)",
		dirfd, func_name, virtual_path);
	res->mres_result_buf = res->mres_result_path = strdup(virtual_path);
	res->mres_readonly = 0;
}

/* this maps the path and then leaves "rule" and "exec_policy" to the stack, 
 * because exec post-processing needs them
*/
void sbox_map_path_for_exec(
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	fwd_map_path(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
		func_name,
		virtual_path, 0/*dont_resolve_final_symlink*/, 1/*exec mode*/, res);
}

char *scratchbox_reverse_path(
	const char *func_name,
	const char *abs_host_path)
{
	char *virtual_path;
	path_mapping_context_t	ctx;

	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = (sbox_binary_name ? sbox_binary_name : "UNKNOWN");
	ctx.pmc_func_name = func_name;
	ctx.pmc_virtual_orig_path = "";
	ctx.pmc_dont_resolve_final_symlink = 0;
	ctx.pmc_luaif = get_sb2context_lua();

	virtual_path = call_lua_function_sbox_reverse_path(&ctx, abs_host_path);
	release_sb2context(ctx.pmc_luaif);
	return(virtual_path);
}

void	clear_mapping_results_struct(mapping_results_t *res)
{
	res->mres_result_buf = res->mres_result_path = NULL;
	res->mres_readonly = 0;
	res->mres_result_path_was_allocated = 0;
	res->mres_errno = 0;
	res->mres_virtual_cwd = NULL;
	res->mres_exec_policy_name = NULL;
	res->mres_error_text = NULL;
}

void	free_mapping_results(mapping_results_t *res)
{
	if (res->mres_result_buf) free(res->mres_result_buf);
	if (res->mres_result_path_was_allocated && res->mres_result_path)
		free(res->mres_result_path);
	if (res->mres_virtual_cwd) free(res->mres_virtual_cwd);
	if (res->mres_exec_policy_name) free(res->mres_exec_policy_name);
	/* res->mres_error_text is a constant string, and not freed, ever */
	clear_mapping_results_struct(res);
}

