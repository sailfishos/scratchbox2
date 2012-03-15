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
#include "processclock.h"

#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif

#include "pathmapping.h" /* get private definitions of this subsystem */

#define MAPPING_METHOD_NOT_SET	 		0
#define MAPPING_METHOD_C_ENGINE	 		1
#define MAPPING_METHOD_LUA_ENGINE		2
#define MAPPING_METHOD_BOTH_ENGINES		3
#define MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS 	4

static int mapping_method = MAPPING_METHOD_NOT_SET;

static struct sb2context *check_mapping_method(int need_sb2ctx)
{
	struct sb2context *sb2ctx = NULL;

	if (mapping_method == MAPPING_METHOD_NOT_SET) {
		if (sbox_mapping_method) {
			if (!strcmp(sbox_mapping_method,"c") ||
			    !strcmp(sbox_mapping_method,"C")) {
				mapping_method = MAPPING_METHOD_C_ENGINE;
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"Selected 'C' mapping method");
			} else if (!strcmp(sbox_mapping_method,"c2") ||
			    !strcmp(sbox_mapping_method,"C2")) {
				mapping_method = MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS;
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"Selected 'C' mapping method");
			} else if (!strcmp(sbox_mapping_method,"Lua") ||
			   	   !strcmp(sbox_mapping_method,"lua")) {
				mapping_method = MAPPING_METHOD_LUA_ENGINE;
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"Selected 'Lua' mapping method");
			} else {
				/* default to both */
				mapping_method = MAPPING_METHOD_BOTH_ENGINES;
				if (strcmp(sbox_mapping_method,"Both") &&
			   	    strcmp(sbox_mapping_method,"both")) {
					SB_LOG(SB_LOGLEVEL_ERROR,
						"Incorrect mapping method (SBOX_MAPPING_METHOD "
						"should contain 'C','Lua' or 'Both'). "
						"Activated both mapping methods");
				}
			}
#if 1
			switch (mapping_method) {
			case MAPPING_METHOD_C_ENGINE:
			case MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS:
				/* A performance optimization for certain binaries:
				 * the shells and "make" do a lot of execs, and as
				 * long as the exec code is still written in Lua,
				 * it is better to load it right away.
				 * (there is a similar performance optimization in
				 * main.lua for these programs)
				 * FIXME: Disable this once the exec logic has been 
				 *        implemented in C.
				*/
				if (sbox_binary_name) {
					if (!strcmp(sbox_binary_name, "sh") ||
					    !strcmp(sbox_binary_name, "bash") ||
					    !strcmp(sbox_binary_name, "make") ||
					    !strcmp(sbox_binary_name, "gmake") ||
					    !strcmp(sbox_binary_name, "xargs") ||
					    !strcmp(sbox_binary_name, "gcc") ||
					    !strcmp(sbox_binary_name, "find")) {
						SB_LOG(SB_LOGLEVEL_DEBUG,
							"opt.perf: binary name '%s' => init lua",
							sbox_binary_name);
						sb2ctx = get_sb2context_lua();
						if (!need_sb2ctx) {
							release_sb2context(sb2ctx);
							sb2ctx = NULL;
						}
					}
				}
			}
#endif
		} else {
			/* default to both */
			mapping_method = MAPPING_METHOD_C_ENGINE;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"(Using C mapping engine = the default)");
		}
	}
	if (need_sb2ctx) {
		switch (mapping_method) {
		case MAPPING_METHOD_C_ENGINE:
		case MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS:
			sb2ctx = get_sb2context();
			break;
		default: /* ..LUA or ..BOTH */
			sb2ctx = get_sb2context_lua();
			break;
		}
	}
	return(sb2ctx);
}

static void compare_results_from_c_and_lua_engines(
	const char *name,
	const char *fn_name,
	const char *c_res,
	const char *lua_res)
{
	if (c_res && lua_res) {
		if (!strcmp(c_res, lua_res)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: ResultCheck: %s same, OK",
				fn_name, name);
		} else {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: ResultCheck: DIFFERENT %s: C='%s', Lua='%s'",
				fn_name, name, c_res, lua_res);
		}
	} else if (!c_res && !lua_res) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: ResultCheck: no %s result from C nor Lua",
			fn_name, name);
	} else {
		if (!c_res) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: ResultCheck: no %s result from C (Lua='%s')",
				fn_name, name, lua_res);
		}
		if (!lua_res) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: ResultCheck: no %s result from Lua (C='%s')",
				fn_name, name, c_res);
		}
	}
}

/* ========== Public interfaces to the mapping & resolution code: ========== */

static void fwd_map_path(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	int exec_mode,
	uint32_t fn_class,
	mapping_results_t *res)
{
	struct sb2context *sb2ctx = NULL;

	(void)exec_mode; /* not used */

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		mapping_results_t res2;
		PROCESSCLOCK(clk1)

		sb2ctx = check_mapping_method(1);

		START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "fwd_map_path");
		switch (mapping_method) {
		case MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS:
			sbox_map_path_internal__c_engine(sb2ctx, binary_name,
				func_name, virtual_path,
				dont_resolve_final_symlink, 0, fn_class, res, 0);
			if (res->mres_fallback_to_lua_mapping_engine) {
				SB_LOG(SB_LOGLEVEL_NOTICE,
					"C path mapping engine failed (%s), fallback to Lua (%s)",
					res->mres_fallback_to_lua_mapping_engine, virtual_path);
				free_mapping_results(res);
				if (!sb2ctx->lua) sb2context_initialize_lua(sb2ctx);
				sbox_map_path_internal__lua_engine(sb2ctx, binary_name,
					func_name, virtual_path,
					dont_resolve_final_symlink, 0, fn_class, res);
			}
			break;
		case MAPPING_METHOD_C_ENGINE:
			sbox_map_path_internal__c_engine(sb2ctx, binary_name,
				func_name, virtual_path,
				dont_resolve_final_symlink, 0, fn_class, res, 0);
			if (res->mres_fallback_to_lua_mapping_engine &&
			    (res->mres_fallback_to_lua_mapping_engine[0] == '#')) {
				SB_LOG(SB_LOGLEVEL_NOTICE,
					"C path mapping engine failed (%s), fallback to Lua was forced (%s)",
					res->mres_fallback_to_lua_mapping_engine, virtual_path);
				free_mapping_results(res);
				if (!sb2ctx->lua) sb2context_initialize_lua(sb2ctx);
				sbox_map_path_internal__lua_engine(sb2ctx, binary_name,
					func_name, virtual_path,
					dont_resolve_final_symlink, 0, fn_class, res);
			} else if (res->mres_fallback_to_lua_mapping_engine) {
				SB_LOG(SB_LOGLEVEL_NOTICE,
					"C path mapping engine failed (%s), NO fallback to Lua (%s)",
					res->mres_fallback_to_lua_mapping_engine, virtual_path);
			}
			break;
		case MAPPING_METHOD_LUA_ENGINE:
			sbox_map_path_internal__lua_engine(sb2ctx, binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, fn_class, res);
			break;
		case MAPPING_METHOD_BOTH_ENGINES:
			clear_mapping_results_struct(&res2);
			sbox_map_path_internal__lua_engine(sb2ctx, binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, fn_class, res);
			sbox_map_path_internal__c_engine(sb2ctx, binary_name, func_name, virtual_path,
				dont_resolve_final_symlink, 0, fn_class, &res2, 0);
			if (res2.mres_fallback_to_lua_mapping_engine) {
				SB_LOG(SB_LOGLEVEL_ERROR,
					"C path mapping engine => fallback to Lua (%s), (%s)",
					res2.mres_fallback_to_lua_mapping_engine, virtual_path);
			}
			compare_results_from_c_and_lua_engines(
				"result path",
				__func__, res2.mres_result_path,
				res->mres_result_path);
			compare_results_from_c_and_lua_engines(
				"virtual cwd",
				__func__, res2.mres_virtual_cwd,
				res->mres_virtual_cwd);
			free_mapping_results(&res2);
			break;
		default:
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Invalid mapping method",
				__func__);
		}
		release_sb2context(sb2ctx);
		STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, virtual_path);
	}
}

#if 0
/* map using a non-standard ruleset. Not used currently,
 * but will be useful for e.g. mapping script interpreters
 * when everything is ready for "full mapping" which
 * includes full path resolution.
*/
void custom_map_path(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	uint32_t fn_class,
	mapping_results_t *res,
	ruletree_object_offset_t rule_list_offset)
{
	struct sb2context *sb2ctx = NULL;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Map %s", __func__, virtual_path);

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		PROCESSCLOCK(clk1)

		sb2ctx = check_mapping_method(1);

		START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, __func__);

		sbox_map_path_internal__c_engine(sb2ctx, binary_name,
			func_name, virtual_path,
			dont_resolve_final_symlink, 0, fn_class, res, rule_list_offset);

		if (res->mres_fallback_to_lua_mapping_engine &&
		    (res->mres_fallback_to_lua_mapping_engine[0] == '#')) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"C path mapping engine failed (%s), can't fallback to Lua (%s) - %s",
				res->mres_fallback_to_lua_mapping_engine, virtual_path, __func__);
		}
	}
	STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, virtual_path);
}
#endif

/* map an "abstract" path: Don't do full path resolution. */
char *custom_map_abstract_path(
	ruletree_object_offset_t rule_list_offs,
	const char *binary_name,
	const char *virtual_orig_path,
	const char *func_name,
	int fn_class,
	const char **new_exec_policy_p)
{
	char			*mapping_result = NULL;
	path_mapping_context_t	ctx;
	struct path_entry_list	abs_virtual_source_path_list;
	int			min_path_len;
	ruletree_object_offset_t	rule_offs;

	if (!virtual_orig_path || (*virtual_orig_path != '/')) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: path '%s' is relative => return(NULL)",
			__func__, virtual_orig_path);
		return(NULL);
	}

	/* fill ctx */
	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = binary_name;
	ctx.pmc_func_name = func_name;
	ctx.pmc_fn_class = fn_class;
	ctx.pmc_virtual_orig_path = virtual_orig_path;
	ctx.pmc_dont_resolve_final_symlink = 0;
	ctx.pmc_sb2ctx = get_sb2context();

	split_path_to_path_list(virtual_orig_path,
		&abs_virtual_source_path_list);

	if (is_clean_path(&abs_virtual_source_path_list) != 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: path '%s' is not a clean path => return(NULL)",
			__func__, virtual_orig_path);
		free_path_list(&abs_virtual_source_path_list);
		return(NULL);
	}

	rule_offs = ruletree_get_mapping_requirements(
		rule_list_offs, &ctx, &abs_virtual_source_path_list,
		&min_path_len, NULL/*call_translate_for_all_p*/,
		SB2_INTERFACE_CLASS_EXEC);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: rule_offs = %u", __func__, rule_offs);

	if (rule_offs) {
		char   		*fallback_to_lua = NULL;
		const char	*new_exec_policy = NULL;
		int		flags;

		ctx.pmc_ruletree_offset = rule_offs;
		mapping_result = ruletree_translate_path(
			&ctx, SB_LOGLEVEL_DEBUG, virtual_orig_path, &flags,
			&new_exec_policy, &fallback_to_lua);
		if (new_exec_policy_p) *new_exec_policy_p = new_exec_policy;
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: mapping_result = %s", __func__,
			(mapping_result ? mapping_result : "NULL"));
	}
	free_path_list(&abs_virtual_source_path_list);

	return(mapping_result);
}

char *reverse_map_path(
	const path_mapping_context_t	*ctx,
	const char *abs_host_path)
{
	char *virtual_path = NULL, *vp2 = NULL;

	if (!abs_host_path || !ctx) {
		return(NULL);
	}

	check_mapping_method(0);

	switch (mapping_method) {
	case MAPPING_METHOD_C_ENGINE_WITH_FALLBACKS:
		virtual_path = sbox_reverse_path_internal__c_engine(
			ctx, abs_host_path);
		if (!virtual_path) {
			/* no answer from the C engine.
			 * path reversing is optional, but we don't even
			 * try to find out if this was caused by missing
			 * rules etc. or failure in the C reversing engine;
			 * just go and try again with Lua.
			*/
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"No result for path reversing from C engine, try Lua (%s)",
					abs_host_path);
			if (!ctx->pmc_sb2ctx->lua) sb2context_initialize_lua(ctx->pmc_sb2ctx);
			virtual_path = call_lua_function_sbox_reverse_path(ctx, abs_host_path);
		}
		break;

	case MAPPING_METHOD_C_ENGINE:
		virtual_path = sbox_reverse_path_internal__c_engine(
			ctx, abs_host_path);
		if (!virtual_path) {
			/* no answer */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"No result for path reversing from C engine (%s)",
					abs_host_path);
		}
		break;

	case MAPPING_METHOD_LUA_ENGINE:
		if (!ctx->pmc_sb2ctx->lua) sb2context_initialize_lua(ctx->pmc_sb2ctx);
		virtual_path = call_lua_function_sbox_reverse_path(ctx, abs_host_path);
		break;

	case MAPPING_METHOD_BOTH_ENGINES:
		if (!ctx->pmc_sb2ctx->lua) sb2context_initialize_lua(ctx->pmc_sb2ctx);
		virtual_path = call_lua_function_sbox_reverse_path(ctx, abs_host_path);
		vp2 = sbox_reverse_path_internal__c_engine(ctx, abs_host_path);
		compare_results_from_c_and_lua_engines(
			"reversed virtual path",
			__func__, vp2, virtual_path);
		if (vp2) free(vp2);
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: Invalid mapping method",
			__func__);
	}
	return(virtual_path);
}

void sbox_map_path_for_sb2show(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	interface_function_and_classes_t	*ifp = interface_functions_and_classes__public;
	uint32_t	fn_class = 0;

	while (ifp && ifp->fn_name) {
		if (!strcmp(func_name, ifp->fn_name)) {
			fn_class = ifp->fn_classmask;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: Found func_class 0x%X (%s)",
				__func__, fn_class, func_name);
			break;
		}
		ifp++;
	}
	if (!fn_class) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: No func_class for %s",
			__func__, func_name);
	}

	fwd_map_path(binary_name, func_name, virtual_path,
		0/*dont_resolve_final_symlink*/, 0/*exec_mode*/, fn_class, res);
}

void sbox_map_path(
	const char *func_name,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	mapping_results_t *res,
	uint32_t classmask)
{
	fwd_map_path(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
		func_name, virtual_path,
		dont_resolve_final_symlink, 0/*exec_mode*/, classmask, res);
}


void sbox_map_path_at(
	const char *func_name,
	int dirfd,
	const char *virtual_path,
	int dont_resolve_final_symlink,
	mapping_results_t *res,
	uint32_t classmask)
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
			dont_resolve_final_symlink, 0/*exec_mode*/, classmask, res);
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
			virtual_abs_path_at_fd, dont_resolve_final_symlink, 0/*exec_mode*/, classmask, res);
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
		virtual_path, 0/*dont_resolve_final_symlink*/, 1/*exec mode*/, 
		SB2_INTERFACE_CLASS_EXEC, res);
}

char *scratchbox_reverse_path(
	const char *func_name,
	const char *abs_host_path,
	uint32_t classmask)
{
	char *virtual_path;
	path_mapping_context_t	ctx;

	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = (sbox_binary_name ? sbox_binary_name : "UNKNOWN");
	ctx.pmc_func_name = func_name;
	ctx.pmc_fn_class = classmask;
	ctx.pmc_virtual_orig_path = "";
	ctx.pmc_dont_resolve_final_symlink = 0;
	ctx.pmc_sb2ctx = get_sb2context();

	virtual_path = reverse_map_path(&ctx, abs_host_path);
	release_sb2context(ctx.pmc_sb2ctx);
	return(virtual_path);
}

void	free_mapping_results(mapping_results_t *res)
{
	if (res->mres_result_buf) free(res->mres_result_buf);
	if (res->mres_result_path_was_allocated && res->mres_result_path)
		free(res->mres_result_path);
	if (res->mres_virtual_cwd) free(res->mres_virtual_cwd);
	if (res->mres_allocated_exec_policy_name) free(res->mres_allocated_exec_policy_name);
	/* res->mres_error_text is a constant string, and not freed, ever */
#if 1
	/* res->mres_fallback_to_lua_mapping_engine is a constant string, and not freed, ever */
#endif
	clear_mapping_results_struct(res);
}

/* manually force the result; used by _nomap versions of GATEs */
void	force_path_to_mapping_result(mapping_results_t *res, const char *path)
{
	res->mres_result_path = res->mres_result_buf = strdup(path);
	res->mres_result_path_was_allocated = 0;
	res->mres_virtual_cwd = NULL;
	res->mres_errno = 0;
}

