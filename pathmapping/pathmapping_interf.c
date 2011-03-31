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

/* ========== Public interfaces to the mapping & resolution code: ========== */

void sbox_map_path_for_sb2show(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		sbox_map_path_internal(binary_name, func_name, virtual_path,
			0/*dont_resolve_final_symlink*/, 0, res);
	}
}

void sbox_map_path(
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	mapping_results_t *res)
{
	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name, virtual_path, dont_resolve_final_symlink, 0, res);
	}
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
		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			virtual_path, dont_resolve_final_symlink, 0, res);
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

		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			virtual_abs_path_at_fd, dont_resolve_final_symlink, 0, res);
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
	sbox_map_path_internal(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"), func_name,
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
	ctx.pmc_luaif = get_lua();

	virtual_path = call_lua_function_sbox_reverse_path(&ctx, abs_host_path);
	release_lua(ctx.pmc_luaif);
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

