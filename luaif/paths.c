/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (symlink- and path resolution code refactored by Lauri T. Aarnio at Nokia)
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * This file implements path resolution for SB2. Please read background
 * information from the "path_resolution" manual page (Linux documentation).
 *
 * Path resolution usually belongs to the operating system, but SB2 must
 * implement a replacement because of one specific feature: Symbolic
 * links must be mapped. For example, if path "/a/b/c" contains
 * intermediate symbolic links (i.e. "a" or "b" is a symlink), the path
 * mapping engine must be called for those all of those symbolic links.
 *
 * TERMINOLOGY:
 *
 * - an _absolute path_ is a path which begins with a slash '/'
 *
 * - a _relative path_ is a path which does not begin with a slash '/',
 *   instead it is relative to the current working directory
 *
 * - a _clean path_ is a path which does not contain a dot ('.',
 *   referring to the current directory), a doubled dot (parent
 *   directory) or doubled slashes (redundant) as a component.
 *
 * - a _virtual path_ is a path that has not been mapped.
 *
 * - a _host path_ is a path that refers to the real filesystem
 *   (i.e. a mapped path)
 *
 * - a _real path_ is an absolute path where none of components is a 
 *   symbolic link and it does not have any . or .. inside
 *   (see realpath(3) for details).
 *   NOTE that within Scratchbox 2 we may have both "host realpaths"
 *   and "virtual realpaths!"
 *
 * - a _resolved path_ is otherwise like a _real path_, but the
 *   last component of a _resolved path_ may be a symbolic link.
 *
 * The division between Lua and C code is that
 * - C has been used for parts that must follow "standardized" or "fixed"
 *   approach, and the implementation is not expected to be changed, in other
 *   words, the implementation/design freedom is restricted by external
 *   requirements.
 * - Lua has been used for the mapping algorithm itself.
*/

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

#include <mapping.h>
#include <sb2.h>
#include "libsb2.h"
#include "exported.h"

#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif

typedef struct path_mapping_context_s {
	const char		*pmc_binary_name;
	const char		*pmc_func_name;
	const char		*pmc_virtual_orig_path;
	int			pmc_dont_resolve_final_symlink;
	struct lua_instance	*pmc_luaif;
} path_mapping_context_t;

#define clear_path_mapping_context(p) {memset((p),0,sizeof(*(p)));}

/* ========== Path & Path component handling primitives: ========== */

struct path_entry {
	struct path_entry *pe_prev;
	struct path_entry *pe_next;

	int	pe_path_component_len;

	/* pe_path_component MUST BE the last member of this
	 * struct, this is really a larger string buffer:
	 * pe_path_component[pe_path_component_len+1] */
	char	pe_path_component[1];
};

struct path_entry_list {
	struct path_entry *pl_first;
};

static struct path_entry *append_path_entries(
	struct path_entry *head,
	struct path_entry *new_entries)
{
	struct path_entry *work = head;
	
	if (!head) return(new_entries);

	/* move work to point to last component of "head" */
	while (work->pe_next) work = work->pe_next;

	work->pe_next = new_entries;
	if (new_entries) new_entries->pe_prev = work;

	return (head);
}

/* returns an allocated buffer */
static char *path_entries_to_string_until(
	const struct path_entry *p_entry,
	const struct path_entry *last_path_entry_to_include)
{
	char *buf;
	const struct path_entry *work;
	int len;

	if (!p_entry) {
		/* "p_entry" will be empty if orig.path was "/." */
		return(strdup("/"));
	}

	/* first, count length of the buffer */
	work = p_entry;
	len = 0;
	while (work) {
		len++; /* "/" */
		len += work->pe_path_component_len;
		if (work == last_path_entry_to_include) break;
		work = work->pe_next;
	}
	len++; /* for trailing \0 */

	buf = malloc(len);
	*buf = '\0';

	/* add path components to the buffer */
	work = p_entry;
	while (work) {
		strcat(buf, "/");
		strcat(buf, work->pe_path_component);
		if (work == last_path_entry_to_include) break;
		work = work->pe_next;
	}

	return(buf);
}

static char *path_entries_to_string(const struct path_entry *p_entry)
{
	return(path_entries_to_string_until(p_entry, NULL));
}

static char *path_list_to_string(const struct path_entry_list *listp)
{
	return(path_entries_to_string_until(listp->pl_first, NULL));
}

static void free_path_entries(struct path_entry *work)
{
	while (work) {
		struct path_entry *next = work->pe_next;
		free(work);
		work = next;
	}
}

static void free_path_list(struct path_entry_list *listp)
{
	free_path_entries(listp->pl_first);
	listp->pl_first = NULL;
}


static struct path_entry *split_path_to_path_entries(
	const char *cpath)
{
	struct path_entry *first = NULL;
	struct path_entry *work = NULL;
	const char *start;
	const char *next_slash;

	SB_LOG(SB_LOGLEVEL_NOISE2, "going to split '%s'", cpath);

	start = cpath;
	while(*start == '/') start++;	/* ignore leading '/' */

	do {
		next_slash = strchr(start, '/');

		/* ignore empty strings resulting from // */
		if (next_slash != start) {
			struct path_entry *new;
			int	len;

			if (next_slash) {
				len = next_slash - start;
			} else {
				/* no more slashes */
				len = strlen(start);
			}
			new = malloc(sizeof(struct path_entry) + len);
			if(!first) first = new;
			if (!new) abort();
			memset(new, 0, sizeof(struct path_entry));
			strncpy(new->pe_path_component, start, len);
			new->pe_path_component[len] = '\0';
			new->pe_path_component_len = len;

			new->pe_prev = work;
			if(work) work->pe_next = new;
			new->pe_next = NULL;
			work = new;
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"created entry 0x%X '%s'",
				(unsigned long int)work, start);
		}
		if (next_slash) start = next_slash + 1;
	} while (next_slash != NULL);

	return (first);
}

static void split_path_to_path_list(
	const char *cpath,
	struct path_entry_list	*listp)
{
	listp->pl_first = split_path_to_path_entries(cpath);

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_list_to_string(listp);

		SB_LOG(SB_LOGLEVEL_NOISE2, "split->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
}

static struct path_entry *duplicate_path_entries_until(
	const struct path_entry *duplicate_until_this_component,
	const struct path_entry *source_path)
{
	struct path_entry *first = NULL;
	struct path_entry *dest_path_ptr = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE2, "Duplicating path:");

	while (source_path) {
		struct path_entry *new;
		int	len = source_path->pe_path_component_len;

		new = malloc(sizeof(struct path_entry) + len);
		if (!new) abort();
		if(!first) first = new;
		memset(new, 0, sizeof(struct path_entry) + len);

		strncpy(new->pe_path_component, source_path->pe_path_component, len);
		new->pe_path_component[len] = '\0';
		new->pe_path_component_len = len;

		new->pe_prev = dest_path_ptr;
		if (dest_path_ptr) dest_path_ptr->pe_next = new;
		new->pe_next = NULL;
		dest_path_ptr = new;
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"dup: entry 0x%X '%s'",
			(unsigned long int)dest_path_ptr, new->pe_path_component);
		if (duplicate_until_this_component == source_path) break;
		source_path = source_path->pe_next;
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_entries_to_string(first);

		SB_LOG(SB_LOGLEVEL_NOISE2, "dup->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
	return(first);
}

static void	duplicate_path_list_until(
	const struct path_entry *duplicate_until_this_component,
	struct path_entry_list *new_path_list,
	const struct path_entry_list *source_path_list)
{
	struct path_entry *duplicate = NULL;

	duplicate = duplicate_path_entries_until(
		duplicate_until_this_component, source_path_list->pl_first);

	new_path_list->pl_first = duplicate;
}

/* remove a path_entry from list, return pointer to the next
 * path_entry after the removed one (NULL if the removed entry was last)
*/
static struct path_entry *remove_path_entry(
	struct path_entry_list *listp,
	struct path_entry *p_entry)	/* entry to be removed */
{
	struct path_entry *ret = p_entry->pe_next;

	if (p_entry->pe_prev) {
		/* not the first element in the list */
		p_entry->pe_prev->pe_next = p_entry->pe_next;
		if(p_entry->pe_next)
			p_entry->pe_next->pe_prev = p_entry->pe_prev;
	} else {
		/* removing first element from the list */
		assert(p_entry == listp->pl_first);
		listp->pl_first = p_entry->pe_next;
		if(p_entry->pe_next)
			p_entry->pe_next->pe_prev = NULL;
	}
	free(p_entry);
	return(ret);
}

static void remove_dots_and_dotdots_from_path_entries(
	struct path_entry_list *listp)
{
	struct path_entry *work = listp->pl_first;

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_list_to_string(listp);

		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: Clean  ->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
	while (work) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: work=0x%X examine '%s'",
			(unsigned long int)work, work?work->pe_path_component:"");
		if (strcmp(work->pe_path_component, "..") == 0) {
			struct path_entry *dotdot = work;
			struct path_entry *preventry = work->pe_prev;

			if (preventry) {
				/* travel up, and eliminate previous name */
				work = remove_path_entry(listp, preventry);
				assert(work == dotdot);
			} else {
				/* no preventry, first component is .. */
				assert(work == listp->pl_first);
			}
			work = remove_path_entry(listp, dotdot);
		} else if (strcmp(work->pe_path_component, ".") == 0) {
			/* ignore this node */
			work = remove_path_entry(listp, work);
		} else {
			work = work->pe_next;
		}
	}
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_list_to_string(listp);

		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: cleaned->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
}

static int is_clean_path(const char *path)
{
	const char	*cp;

	/* check if there are double slashes */
	if (*path == '/') cp = path;
	else cp = strchr(path, '/');
	while(cp) {
		if (cp[1] == '/') {
			/* found "//" */
			SB_LOG(SB_LOGLEVEL_NOISE,
				"is_clean_path: false; double slash '%s'",
				path);
			return(0);
		}
		cp = strchr(cp+1, '/');
	}

	/* check if the path contains "." or ".." as components */
	cp = path;
	while (*cp == '/') cp++;
	while (cp) {
		if (*cp == '.') {
			if ((cp[1] == '/') ||
			    (cp[1] == '\0') ||
			    (cp[1] == '.' && cp[2] == '/') ||
			    (cp[1] == '.' && cp[2] == '\0')) {
				/* found "." or ".." */
				SB_LOG(SB_LOGLEVEL_NOISE,
					"is_clean_path: false; dots '%s'",
					path);
				return(0);
			}
		}
		cp = strchr(cp+1, '/');
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "is_clean_path: true; '%s'", path);
	return(1);
}

/* returns an allocated buffer containing a cleaned ("decolonized")
 * version of "path" (double slashes, dots and dotdots etc. have been removed)
*/
static char *sb_clean_path(const char *path)
{
	struct path_entry_list list;
	char *buf = NULL;

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_clean_path called with NULL path");
		return NULL;
	}

	if (is_clean_path(path)) return(strdup(path));

	/* path needs cleaning */
	split_path_to_path_list(path, &list);
	remove_dots_and_dotdots_from_path_entries(&list);

	buf = path_list_to_string(&list);
	free_path_list(&list);

	SB_LOG(SB_LOGLEVEL_NOISE, "sb_clean_path returns '%s'", buf);
	return buf;
}

/* ========== Other helper functions: ========== */

static char last_char_in_str(const char *str)
{
	if (!str || !*str) return('\0');
	while (str[1]) str++;
	return(*str);
}

static void check_mapping_flags(int flags, const char *fn)
{
	if (flags & (~SB2_MAPPING_RULE_ALL_FLAGS)) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s returned unknown flags (0%o)",
			fn, flags & (~SB2_MAPPING_RULE_ALL_FLAGS));
	}
}

/* ========== Interfaces to Lua functions: ========== */

/* note: this expects that the lua stack already contains the mapping rule,
 * needed by sbox_translate_path (lua code).
 * at exit this always leaves the rule AND exec policy to stack!
*/
static char *call_lua_function_sbox_translate_path(
	const path_mapping_context_t *ctx,
	int result_log_level,
	const char *abs_clean_virtual_path,
	int *flagsp)
{
	struct lua_instance	*luaif = ctx->pmc_luaif;
	int flags;
	char *host_path = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_translate_path for %s(%s)",
		ctx->pmc_func_name, abs_clean_virtual_path);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_translate_path: gettop=%d",
		lua_gettop(luaif->lua));
	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_sbox_translate_path entry",
			luaif->lua);
	}

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	/* stack now contains the rule object and string "sbox_translate_path",
         * move the string to the bottom: */
	lua_insert(luaif->lua, -2);
	/* add other parameters */
	lua_pushstring(luaif->lua, ctx->pmc_binary_name);
	lua_pushstring(luaif->lua, ctx->pmc_func_name);
	lua_pushstring(luaif->lua, abs_clean_virtual_path);
	 /* 4 arguments, returns rule,policy,path,flags */
	lua_call(luaif->lua, 4, 4);

	host_path = (char *)lua_tostring(luaif->lua, -2);
	if (host_path && (*host_path != '/')) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Mapping failed: Result is not absolute ('%s'->'%s')",
			abs_clean_virtual_path, host_path);
		host_path = NULL;
	} else if (host_path) {
		host_path = strdup(host_path);
	}
	flags = lua_tointeger(luaif->lua, -1);
	check_mapping_flags(flags, "sbox_translate_path");
	if (flagsp) *flagsp = flags;
	lua_pop(luaif->lua, 2); /* leave rule and policy to the stack */

	if (host_path) {
		/* sometimes a mapping rule may create paths that contain
		 * doubled slashes ("//") or end with a slash. We'll
		 * need to clean the path here.
		*/
		char *cleaned_host_path;

		cleaned_host_path = sb_clean_path(host_path);

		if (*cleaned_host_path != '/') {
			/* oops, got a relative path. CWD is too long. */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"OOPS, call_lua_function_sbox_translate_path:"
				" relative");
		}
		free(host_path);
		host_path = NULL;

		/* log the result */
		if (strcmp(cleaned_host_path, abs_clean_virtual_path) == 0) {
			/* NOTE: Following SB_LOG() call is used by the log
			 *       postprocessor script "sb2logz". Do not change
			 *       without making a corresponding change to
			 *       the script!
			*/
			SB_LOG(result_log_level, "pass: %s '%s'%s",
				ctx->pmc_func_name, abs_clean_virtual_path,
				((flags & SB2_MAPPING_RULE_FLAGS_READONLY) ? " (readonly)" : ""));
		} else {
			/* NOTE: Following SB_LOG() call is used by the log
			 *       postprocessor script "sb2logz". Do not change
			 *       without making a corresponding change to
			 *       the script!
			*/
			SB_LOG(result_log_level, "mapped: %s '%s' -> '%s'%s",
				ctx->pmc_func_name, abs_clean_virtual_path, cleaned_host_path,
				((flags & SB2_MAPPING_RULE_FLAGS_READONLY) ? " (readonly)" : ""));
		}
		host_path = cleaned_host_path;
	}
	if (!host_path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"No result from sbox_translate_path for: %s '%s'",
			ctx->pmc_func_name, abs_clean_virtual_path);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_translate_path: at exit, gettop=%d",
		lua_gettop(luaif->lua));
	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_sbox_translate_path exit",
			luaif->lua);
	}
	return(host_path);
}

/* - returns 1 if ok (then *min_path_lenp is valid)
 * - returns 0 if failed to find the rule
 * Note: this leave the rule to the stack!
*/
static int call_lua_function_sbox_get_mapping_requirements(
	const path_mapping_context_t *ctx,
	const char *abs_virtual_source_path,
	int *min_path_lenp,
	int *call_translate_for_all_p)
{
	struct lua_instance	*luaif = ctx->pmc_luaif;
	int rule_found;
	int min_path_len;
	int flags;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"calling sbox_get_mapping_requirements for %s(%s)",
		ctx->pmc_func_name, abs_virtual_source_path);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_get_mapping_requirements: gettop=%d",
		lua_gettop(luaif->lua));

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX,
		"sbox_get_mapping_requirements");
	lua_pushstring(luaif->lua, ctx->pmc_binary_name);
	lua_pushstring(luaif->lua, ctx->pmc_func_name);
	lua_pushstring(luaif->lua, abs_virtual_source_path);
	/* 3 arguments, returns 4: (rule, rule_found_flag,
	 * min_path_len, flags) */
	lua_call(luaif->lua, 3, 4);

	rule_found = lua_toboolean(luaif->lua, -3);
	min_path_len = lua_tointeger(luaif->lua, -2);
	flags = lua_tointeger(luaif->lua, -1);
	check_mapping_flags(flags, "sbox_get_mapping_requirements");
	if (min_path_lenp) *min_path_lenp = min_path_len;
	if (call_translate_for_all_p)
		*call_translate_for_all_p =
			(flags & SB2_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL);

	/* remove last 3 values; leave "rule" to the stack */
	lua_pop(luaif->lua, 3);

	SB_LOG(SB_LOGLEVEL_DEBUG, "sbox_get_mapping_requirements -> %d,%d,0%o",
		rule_found, min_path_len, flags);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_get_mapping_requirements:"
		" at exit, gettop=%d",
		lua_gettop(luaif->lua));
	return(rule_found);
}

/* returns virtual_path */
static char *call_lua_function_sbox_reverse_path(
	const path_mapping_context_t *ctx,
	const char *abs_host_path)
{
	struct lua_instance	*luaif = ctx->pmc_luaif;
	char *virtual_path = NULL;
	int flags;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_reverse_path for %s(%s)",
		ctx->pmc_func_name, abs_host_path);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_reverse_path");
	lua_pushstring(luaif->lua, ctx->pmc_binary_name);
	lua_pushstring(luaif->lua, ctx->pmc_func_name);
	lua_pushstring(luaif->lua, abs_host_path);
	 /* 3 arguments, returns virtual_path and flags */
	lua_call(luaif->lua, 3, 2);

	virtual_path = (char *)lua_tostring(luaif->lua, -2);
	if (virtual_path) {
		virtual_path = strdup(virtual_path);
	}

	flags = lua_tointeger(luaif->lua, -1);
	check_mapping_flags(flags, "sbox_reverse_path");
	/* Note: "flags" is not yet used for anything, intentionally */
 
	lua_pop(luaif->lua, 2); /* remove return values */

	if (virtual_path) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "virtual_path='%s'", virtual_path);
	} else {
		SB_LOG(SB_LOGLEVEL_INFO,
			"No result from sbox_reverse_path for: %s '%s'",
			ctx->pmc_func_name, abs_host_path);
	}
	return(virtual_path);
}

/* ========== Path resolution: ========== */

/* clean up path resolution environment from lua stack */
#define drop_policy_from_lua_stack(luaif) drop_from_lua_stack(luaif,"policy")
#define drop_rule_from_lua_stack(luaif) drop_from_lua_stack(luaif,"rule")

static void drop_from_lua_stack(struct lua_instance *luaif,
	const char *o_name)
{
	lua_pop(luaif->lua, 1);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"drop %s from stack: at exit, gettop=%d", o_name,
		lua_gettop(luaif->lua));
}

static void sb_path_resolution_resolve_symlink(
	const path_mapping_context_t *ctx,
	const char *link_dest,
	const struct path_entry_list *virtual_source_path_list,
	const struct path_entry *virtual_path_work_ptr,
	mapping_results_t *resolved_virtual_path_res,
	int nest_count);

/* sb_path_resolution():  This is the place where symlinks are followed.
 *
 * Returns an allocated buffer containing the resolved path (or NULL if error)
 *
 * Note: when this function returns, lua stack contains the rule which was
 *       used to do the path resolution. drop_rule_from_lua_stack() must
 *       be called after it is not needed anymore!
*/
static void sb_path_resolution(
	const path_mapping_context_t *ctx,
	mapping_results_t *resolved_virtual_path_res,
	int nest_count,
	const char *abs_virtual_source_path)	/* MUST be an absolute path! */
{
	struct path_entry_list virtual_source_path_list;
	struct path_entry *virtual_path_work_ptr;
	int	component_index = 0;
	int	min_path_len_to_check;
	char	*prefix_mapping_result_host_path;
	int	prefix_mapping_result_host_path_flags;
	int	call_translate_for_all = 0;

	if (nest_count > 16) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Detected too deep nesting "
			"(too many symbolic links, path='%s')",
			abs_virtual_source_path);

		/* return ELOOP to the calling program */
		resolved_virtual_path_res->mres_errno = ELOOP;
		return;
	}

	if (!abs_virtual_source_path || !*abs_virtual_source_path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_path_resolution called with NULL path");
		return;
	}
	if (*abs_virtual_source_path != '/') {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"FATAL: sb_path_resolution called with relative path");
		assert(0);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution %d '%s'", nest_count, abs_virtual_source_path);

	virtual_source_path_list.pl_first = NULL;

	split_path_to_path_list(abs_virtual_source_path, &virtual_source_path_list);

	virtual_path_work_ptr = virtual_source_path_list.pl_first;

	if (call_lua_function_sbox_get_mapping_requirements(
		ctx, abs_virtual_source_path,
		&min_path_len_to_check, &call_translate_for_all)) {
		/* has requirements:
		 * skip over path components that we are not supposed to check,
		 * because otherwise rule recognition & execution could fail.
		*/
		int	skipped_len = 1; /* start from 1, abs path has '/' in the beginning */
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"min_path_len_to_check=%d", min_path_len_to_check);

		while (skipped_len < min_path_len_to_check) {
			SB_LOG(SB_LOGLEVEL_NOISE2, "skipping [%d] '%s'",
				component_index, virtual_path_work_ptr->pe_path_component);
			component_index++;
			skipped_len += virtual_path_work_ptr->pe_path_component_len;
			skipped_len++; /* add one due to the slash which lays between components */
			virtual_path_work_ptr = virtual_path_work_ptr->pe_next;
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "Path resolutions starts from [%d] '%s'",
		component_index, virtual_path_work_ptr->pe_path_component);

	/* abs_virtual_source_path is not necessarily clean.
	 * it may contain . or .. as a result of symbolic link expansion
	 * (i.e. when this function is called recursively) */
	{
		char	*clean_virtual_path_prefix_tmp;
		struct path_entry_list virtual_prefix_path_list;
		path_mapping_context_t	ctx_copy = *ctx;

		ctx_copy.pmc_binary_name = "PATH_RESOLUTION";

		virtual_prefix_path_list.pl_first = NULL;
		duplicate_path_list_until(virtual_path_work_ptr,
			&virtual_prefix_path_list, &virtual_source_path_list);
		remove_dots_and_dotdots_from_path_entries(&virtual_prefix_path_list);
		clean_virtual_path_prefix_tmp = path_list_to_string(&virtual_prefix_path_list);
		free_path_list(&virtual_prefix_path_list);

		SB_LOG(SB_LOGLEVEL_NOISE, "clean_virtual_path_prefix_tmp => %s",
			clean_virtual_path_prefix_tmp);

		prefix_mapping_result_host_path = call_lua_function_sbox_translate_path(
			&ctx_copy, SB_LOGLEVEL_NOISE,
			clean_virtual_path_prefix_tmp, &prefix_mapping_result_host_path_flags);
		drop_policy_from_lua_stack(ctx->pmc_luaif);
		free(clean_virtual_path_prefix_tmp);
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "prefix_mapping_result_host_path before loop => %s",
		prefix_mapping_result_host_path);

	/* Path resolution loop = walk thru directories, and if a symlink
	 * is found, recurse..
	*/
	while (virtual_path_work_ptr) {
		char	link_dest[PATH_MAX+1];
		int	link_len;

		if (prefix_mapping_result_host_path_flags &
		    SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH) {
			/* "force_orig_path" is set when symlinks MUST NOT
			 * be followed. */
			SB_LOG(SB_LOGLEVEL_NOISE,
				"force_orig_path set => path resolution finished");
			break;
		}

		if (ctx->pmc_dont_resolve_final_symlink && (virtual_path_work_ptr->pe_next == NULL)) {
			/* this is last component, but here a final symlink
			 * must not be resolved (calls like lstat(), rename(),
			 * etc)
			*/
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"Won't check last component [%d] '%s'",
				component_index, virtual_path_work_ptr->pe_path_component);
			break;
		}

		SB_LOG(SB_LOGLEVEL_NOISE, "path_resolution: test if symlink [%d] '%s'",
			component_index, prefix_mapping_result_host_path);

		/* determine if "prefix_mapping_result_host_path" is a symbolic link.
		 * this can't be done with lstat(), because lstat() does not
		 * exist as a function on Linux => lstat_nomap() can not be
		 * used eiher. fortunately readlink() is an ordinary function.
		*/
		link_len = readlink_nomap(prefix_mapping_result_host_path, link_dest, PATH_MAX);

		if (link_len > 0) {
			/* was a symlink */

			free(prefix_mapping_result_host_path);

			link_dest[link_len] = '\0';
			SB_LOG(SB_LOGLEVEL_NOISE,
				"Path resolution found symlink '%s' "
				"-> '%s'",
				prefix_mapping_result_host_path, link_dest);
			sb_path_resolution_resolve_symlink(ctx, link_dest,
				&virtual_source_path_list, virtual_path_work_ptr,
				resolved_virtual_path_res, nest_count);
			free_path_list(&virtual_source_path_list);
			return;
		}

		/* not a symlink */
		virtual_path_work_ptr = virtual_path_work_ptr->pe_next;
		if (virtual_path_work_ptr) {
			if (call_translate_for_all) {
				/* call_translate_for_all is set when
				 * path resolution must call
				 * sbox_translate_path() for each component;
				 * this happens when a "custom_map_funct" has
				 * been set. "custom_map_funct" may use any
				 * kind of strategy to decide when mapping
				 * needs to be done, for example, the /proc
				 * mapping function looks at the suffix, and
				 * not at the prefix...
				*/
				char *virtual_path_prefix_to_map;
				path_mapping_context_t	ctx_copy = *ctx;

				ctx_copy.pmc_binary_name = "PATH_RESOLUTION/2";
				if (prefix_mapping_result_host_path) {
					free(prefix_mapping_result_host_path);
				}
				virtual_path_prefix_to_map = path_entries_to_string_until(
						virtual_source_path_list.pl_first,
						virtual_path_work_ptr);
				prefix_mapping_result_host_path =
					call_lua_function_sbox_translate_path(
						&ctx_copy, SB_LOGLEVEL_NOISE,
						virtual_path_prefix_to_map,
						&prefix_mapping_result_host_path_flags);
				free (virtual_path_prefix_to_map);
				drop_policy_from_lua_stack(ctx->pmc_luaif);
			} else {
				/* "standard mapping", based on prefix or
				 * exact match. Ok to skip sbox_translate_path()
				 * because here it would just add the component
				 * to end of the path; instead we'll do that
				 * here. This is a performance optimization.
				*/
				char	*next_dir = NULL;

				if (asprintf(&next_dir, "%s/%s",
					prefix_mapping_result_host_path,
					virtual_path_work_ptr->pe_path_component) < 0) {
					SB_LOG(SB_LOGLEVEL_ERROR,
						"asprintf failed");
				}
				if (prefix_mapping_result_host_path) {
					free(prefix_mapping_result_host_path);
				}
				prefix_mapping_result_host_path = next_dir;
			}
		} else {
			free(prefix_mapping_result_host_path);
		}
		component_index++;
	}

	/* All symbolic links have been resolved.
	 *
	 * Since there are no symlinks in "virtual_source_path_list", "." and ".."
	 * entries can be safely removed:
	*/
	{
		char	*resolved_virtual_path_buf = NULL;

		remove_dots_and_dotdots_from_path_entries(&virtual_source_path_list);
		resolved_virtual_path_buf = path_list_to_string(&virtual_source_path_list);
		free_path_list(&virtual_source_path_list);

		SB_LOG(SB_LOGLEVEL_NOISE,
			"sb_path_resolution returns '%s'", resolved_virtual_path_buf);
		resolved_virtual_path_res->mres_result_buf =
			resolved_virtual_path_res->mres_result_path =
			resolved_virtual_path_buf;
	}
}

static void sb_path_resolution_resolve_symlink(
	const path_mapping_context_t *ctx,
	const char *link_dest,
	const struct path_entry_list *virtual_source_path_list,
	const struct path_entry *virtual_path_work_ptr,
	mapping_results_t *resolved_virtual_path_res,
	int nest_count)
{
	char	*new_abs_virtual_link_dest_path = NULL;
	struct path_entry *rest_of_virtual_path = NULL;

	if (virtual_path_work_ptr->pe_next) {
		/* path has components after the symlink:
		 * duplicate remaining path, it needs to 
		 * be attached to symlink contents. 
		*/
		rest_of_virtual_path = duplicate_path_entries_until(
			NULL, virtual_path_work_ptr->pe_next);
	} /* else last component of the path was a symlink. */

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE) && rest_of_virtual_path) {
		char *tmp_path_buf = path_entries_to_string(rest_of_virtual_path);
		SB_LOG(SB_LOGLEVEL_NOISE2, "resolve_symlink: rest='%s'", tmp_path_buf);
		free(tmp_path_buf);
	}

	if (*link_dest == '/') {
		/* absolute symlink.
		 * This is easy: just join the symlink
		 * and rest of path, and further mapping
		 * operations will take care of the rest.
		*/
		if (rest_of_virtual_path) {
			struct path_entry *symlink_entries = NULL;

			SB_LOG(SB_LOGLEVEL_NOISE, "absolute symlink");
			symlink_entries = split_path_to_path_entries(link_dest);
			append_path_entries(symlink_entries, rest_of_virtual_path);
			rest_of_virtual_path = NULL;
			new_abs_virtual_link_dest_path = path_entries_to_string(
				symlink_entries);
			free_path_entries(symlink_entries);
		} else {
			new_abs_virtual_link_dest_path = strdup(link_dest);
		}
	} else {
		/* A relative symlink. Somewhat complex:
		 * "prefix_mapping_result_host_path" contains the
		 * real location in the FS, but here we
		 * must still build the full path from the
		 * place where we pretend to be - otherwise
		 * path mapping code would fail to find the
		 * correct location. Hence "dirnam" is
		 * based on what was mapped, and not based on
		 * were the mapping took us.
		*/
		struct path_entry *symlink_entries;
		struct path_entry *link_dest_entries;

		SB_LOG(SB_LOGLEVEL_NOISE, "relative symlink");
		/* first, set symlink_entries to be
		 * the path to the parent directory.
		*/
		if (virtual_path_work_ptr->pe_prev) {
			symlink_entries = duplicate_path_entries_until(
				virtual_path_work_ptr->pe_prev,
				virtual_source_path_list->pl_first);
		} else {
			/* else parent directory = rootdir, 
			 * symlink_entries=NULL signifies that. */
			symlink_entries = NULL;
		}

		link_dest_entries = split_path_to_path_entries(link_dest);
		symlink_entries = append_path_entries(
			symlink_entries, link_dest_entries);
		link_dest_entries = NULL;

		if (rest_of_virtual_path) {
			symlink_entries = append_path_entries(
				symlink_entries, rest_of_virtual_path);
			rest_of_virtual_path = NULL;;
		}

		new_abs_virtual_link_dest_path = path_entries_to_string(
			symlink_entries);
		free_path_entries(symlink_entries);
	}

	/* double-check the result. We MUST use absolute
	 * paths here.
	*/
	if (*new_abs_virtual_link_dest_path != '/') {
		/* this should never happen */
		SB_LOG(SB_LOGLEVEL_ERROR,
			"FATAL: symlink resolved to "
			"a relative path (internal error)");
		assert(0);
	}

	/* recursively call sb_path_resolution() to perform path
	 * resolution steps for the symlink target.
	*/

	/* First, forget the old rule: */
	drop_rule_from_lua_stack(ctx->pmc_luaif);

	/* Then the recursion.
	 * NOTE: new_abs_virtual_link_dest_path is not necessarily
	 * a clean path, because the symlink may have pointed to .. */
	sb_path_resolution(ctx, resolved_virtual_path_res, nest_count + 1,
		new_abs_virtual_link_dest_path);

	/* and finally, cleanup */
	free(new_abs_virtual_link_dest_path);
	return;
}

/* ========== Mapping & path resolution, internal implementation: ========== */

static const char *relative_virtual_path_to_abs_path(
	const path_mapping_context_t *ctx,
	char *host_cwd,
	size_t host_cwd_size,
	const char *relative_virtual_path,
	char **abs_virtual_path_buffer_p)
{
	struct lua_instance	*luaif = ctx->pmc_luaif;
	char *virtual_reversed_cwd = NULL;

	if (!getcwd_nomap_nolog(host_cwd, host_cwd_size)) {
		/* getcwd() returns NULL if the path is really long.
		 * In this case the path can not be mapped.
		 *
		 * Added 2009-01-16: This actually happens sometimes;
		 * there are configure scripts that find out MAX_PATH 
		 * the hard way. So, if this process has already
		 * logged this error, we'll suppress further messages.
		 * [decided to add this check after I had seen 3686
		 * error messages from "conftest" :-) /LTA]
		*/
		static int absolute_path_failed_message_logged = 0;

		if (!absolute_path_failed_message_logged) {
			absolute_path_failed_message_logged = 1;
			SB_LOG(SB_LOGLEVEL_ERROR,
			    "absolute_path failed to get current dir");
		}
		return(NULL);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"sbox_map_path_internal: converting to abs.path cwd=%s",
		host_cwd);
	
	/* reversing of paths is expensive...try if a previous
	 * result can be used, and call the reversing logic only if
	 * CWD has been changed.
	*/
	if (luaif->host_cwd && luaif->virtual_reversed_cwd &&
	    !strcmp(host_cwd, luaif->host_cwd)) {
		/* "cache hit" */
		virtual_reversed_cwd = luaif->virtual_reversed_cwd;
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: using cached rev_cwd=%s",
			virtual_reversed_cwd);
	} else {
		/* "cache miss" */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: reversing cwd:");
		virtual_reversed_cwd = call_lua_function_sbox_reverse_path(
			ctx, host_cwd);
		if (virtual_reversed_cwd == NULL) {
			/*
			 * In case reverse path couldn't be resolved
			 * we fallback into host_cwd.  This is the
			 * way it used to work before.
			 */
			virtual_reversed_cwd = strdup(host_cwd);
			SB_LOG(SB_LOGLEVEL_DEBUG,
			    "unable to reverse, using reversed_cwd=%s",
			    virtual_reversed_cwd);
		}
		/* put the reversed CWD to our one-slot cache: */
		if (luaif->host_cwd) free(luaif->host_cwd);
		if (luaif->virtual_reversed_cwd) free(luaif->virtual_reversed_cwd);
		luaif->host_cwd = strdup(host_cwd);
		luaif->virtual_reversed_cwd = virtual_reversed_cwd;
	}
	if (asprintf(abs_virtual_path_buffer_p, "%s/%s",
	     virtual_reversed_cwd, relative_virtual_path) < 0) {
		/* asprintf failed */
		abort();
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"sbox_map_path_internal: abs.path is '%s'",
		*abs_virtual_path_buffer_p);

	return(*abs_virtual_path_buffer_p);
}

/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function.
 * Returns a pointer to an allocated buffer which contains the result.
 */
static void sbox_map_path_internal(
	const char *binary_name,
	const char *func_name,
	const char *virtual_orig_path,
	int dont_resolve_final_symlink,
	int process_path_for_exec,
	mapping_results_t *res)
{
	char *mapping_result = NULL;
	const char *abs_virtual_path = NULL;
	char *abs_virtual_path_buffer = NULL;
	path_mapping_context_t	ctx;
	char host_cwd[PATH_MAX + 1]; /* used only if virtual_orig_path is relative */

	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = binary_name;
	ctx.pmc_func_name = func_name;
	ctx.pmc_virtual_orig_path = virtual_orig_path;
	ctx.pmc_dont_resolve_final_symlink = dont_resolve_final_symlink;

	SB_LOG(SB_LOGLEVEL_DEBUG, "sbox_map_path_internal: %s(%s)", func_name, virtual_orig_path);

#ifdef EXTREME_DEBUGGING
	#define SIZE 100
	void *buffer[SIZE];
	char **strings;
	int i, nptrs;

	nptrs = backtrace(buffer, SIZE);
	strings = backtrace_symbols(buffer, nptrs);
	for (i = 0; i < nptrs; i++)
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s\n", strings[i]);
#endif
	if (!virtual_orig_path || !*virtual_orig_path) {
		/* an empty path shall always remain empty */
		res->mres_result_buf = res->mres_result_path = strdup("");
		return;
	}

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "disabled(E): %s '%s'",
			func_name, virtual_orig_path);
		goto use_orig_path_as_result_and_exit;
	}

	ctx.pmc_luaif = get_lua();
	if (!ctx.pmc_luaif) {
		/* init in progress? */
		goto use_orig_path_as_result_and_exit;
	}
	if (ctx.pmc_luaif->mapping_disabled) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "disabled(%d): %s '%s'",
			ctx.pmc_luaif->mapping_disabled, func_name, virtual_orig_path);
		goto use_orig_path_as_result_and_exit;
	}

	/* Going to map it. The mapping logic must get absolute paths: */
	if (*virtual_orig_path == '/') {
		abs_virtual_path = virtual_orig_path;
	} else {
		/* convert to absolute path. */
		abs_virtual_path = relative_virtual_path_to_abs_path(
			&ctx, host_cwd, sizeof(host_cwd),
			virtual_orig_path, &abs_virtual_path_buffer);
		if (!abs_virtual_path)
			goto use_orig_path_as_result_and_exit;
	}

	disable_mapping(ctx.pmc_luaif);
	{
		/* Mapping disabled inside this block - do not use "return"!! */
		mapping_results_t	resolved_virtual_path_res;
		char			*clean_abs_virtual_path_for_rule_selection = NULL;

		clear_mapping_results_struct(&resolved_virtual_path_res);

		clean_abs_virtual_path_for_rule_selection = sb_clean_path(abs_virtual_path);

		if (!clean_abs_virtual_path_for_rule_selection ||
		    (*clean_abs_virtual_path_for_rule_selection != '/')) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"sbox_map_path_internal: sb_clean_path()"
				" failed to return absolute path (can't"
				" map this)");
			mapping_result = strdup(abs_virtual_path);
			if (process_path_for_exec) {
				/* can't map, but still need to leave "rule"
				 * (string) and "policy" (nil) to the stack */
				lua_pushstring(ctx.pmc_luaif->lua,
					"mapping failed (failed to make it absolute)");
				lua_pushnil(ctx.pmc_luaif->lua);
			}
			goto forget_mapping;
		}

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: process '%s', n='%s'",
			virtual_orig_path, clean_abs_virtual_path_for_rule_selection);

		/* sb_path_resolution() leaves the rule to the stack... */
		sb_path_resolution(&ctx, &resolved_virtual_path_res, 0,
			clean_abs_virtual_path_for_rule_selection);

		if (resolved_virtual_path_res.mres_errno) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"path mapping fails, "
				" errno = %d",
				resolved_virtual_path_res.mres_errno);
			res->mres_errno = resolved_virtual_path_res.mres_errno;
			goto forget_mapping;
		}

		if (!resolved_virtual_path_res.mres_result_path) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"sbox_map_path_internal: "
				"path resolution failed [%s]",
				func_name);
			mapping_result = NULL;
			if (process_path_for_exec) {
				/* can't map, but still need to leave "rule"
				 * (string) and "policy" (nil) to the stack */
				lua_pushstring(ctx.pmc_luaif->lua, 
					"mapping failed (path resolution path failed)");
				lua_pushnil(ctx.pmc_luaif->lua);
			}
		} else {
			int	flags;

			SB_LOG(SB_LOGLEVEL_NOISE2,
				"sbox_map_path_internal: resolved_virtua='%s'",
				resolved_virtual_path_res.mres_result_path);

			mapping_result = call_lua_function_sbox_translate_path(
				&ctx, SB_LOGLEVEL_INFO,
				resolved_virtual_path_res.mres_result_path, &flags);
			res->mres_readonly = (flags & SB2_MAPPING_RULE_FLAGS_READONLY);

			if (process_path_for_exec == 0) {
				/* ...and remove rule and policy from stack */
				drop_policy_from_lua_stack(ctx.pmc_luaif);
				drop_rule_from_lua_stack(ctx.pmc_luaif);
			}
		}
	forget_mapping:
		free_mapping_results(&resolved_virtual_path_res);
		if(clean_abs_virtual_path_for_rule_selection)
			free(clean_abs_virtual_path_for_rule_selection);
	}
	enable_mapping(ctx.pmc_luaif);

	res->mres_result_buf = res->mres_result_path = mapping_result;

	/* now "mapping_result" is an absolute path.
	 * sb2's exec logic needs absolute paths, and absolute paths are also
	 * needed when registering paths to the "fdpathdb".  but otherwise,
	 * we'll try to return a relative path if the original path was
	 * relative (abs.path is still available in mres_result_buf).
	*/
	if ((process_path_for_exec == 0) &&
	    (virtual_orig_path[0] != '/') &&
	    mapping_result &&
	    (*mapping_result == '/')) {
		/* virtual_orig_path was relative. host_cwd has been filled above */
		int	host_cwd_len = strlen(host_cwd);
		int	result_len = strlen(mapping_result);

		if ((result_len == host_cwd_len) &&
		    !strcmp(host_cwd, mapping_result)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: result==CWD");
			res->mres_result_path = strdup(".");
			res->mres_result_path_was_allocated = 1;
		} else if ((result_len > host_cwd_len) &&
			   (mapping_result[host_cwd_len] == '/') &&
			   (mapping_result[host_cwd_len+1] != '/') &&
			   (mapping_result[host_cwd_len+1] != '\0') &&
			   !strncmp(host_cwd, mapping_result, host_cwd_len)) {
			/* host_cwd is a prefix of result; convert result
			 * back to a relative path
			*/
			char *relative_result = mapping_result+host_cwd_len+1;
			res->mres_result_path = relative_result;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: result==relative (%s) (%s)",
				relative_result, mapping_result);
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "sbox_map_path_internal: mapping_result='%s'",
		mapping_result ? mapping_result : "<No result>");
	release_lua(ctx.pmc_luaif);
	if (abs_virtual_path_buffer) free(abs_virtual_path_buffer);
	return;

    use_orig_path_as_result_and_exit:
	if(ctx.pmc_luaif) release_lua(ctx.pmc_luaif);
	res->mres_result_buf = res->mres_result_path = strdup(virtual_orig_path);
	return;
}

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
}

void	free_mapping_results(mapping_results_t *res)
{
	if (res->mres_result_buf) free(res->mres_result_buf);
	if (res->mres_result_path_was_allocated && res->mres_result_path)
		free(res->mres_result_path);
	clear_mapping_results_struct(res);
}

