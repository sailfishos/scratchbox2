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

/* ========== Path & Path component handling primitives: ========== */

struct path_entry {
	struct path_entry *pe_prev;
	struct path_entry *pe_next;
	char *pe_last_component_name;
	char *pe_full_path;
};

struct path_entry_list {
	struct path_entry *pl_first;
};

/* returns an allocated buffer */
static char *path_entries_to_string(struct path_entry *p_entry)
{
	char *buf;
	struct path_entry *work;
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
		len += strlen(work->pe_last_component_name);
		work = work->pe_next;
	}
	len++; /* for trailing \0 */

	buf = malloc(len);
	*buf = '\0';

	/* add path components to the buffer */
	work = p_entry;
	while (work) {
		strcat(buf, "/");
		strcat(buf, work->pe_last_component_name);
		work = work->pe_next;
	}

	return(buf);
}

static void free_path_entries(struct path_entry_list *listp)
{
	struct path_entry *work;

	work = listp->pl_first;

	while (work) {
		struct path_entry *tmp;
		tmp = work;
		work = work->pe_next;
		if (tmp->pe_full_path) free(tmp->pe_full_path);
		free(tmp);
	}

	listp->pl_first = NULL;
}

static void split_path_to_path_entries(
	const char *cpath,
	struct path_entry_list	*listp)
{
	struct path_entry *work = NULL;
	const char *start;
	const char *next_slash;

	SB_LOG(SB_LOGLEVEL_NOISE2, "going to split '%s'", cpath);

	listp->pl_first = NULL;

	start = cpath;
	while(*start == '/') start++;	/* ignore leading '/' */

	do {
		next_slash = strchr(start, '/');

		/* ignore empty strings resulting from // */
		if (next_slash != start) {
			struct path_entry *new;

			/* add an entry to our path_entry list */
			if (!(new = calloc(1,sizeof(struct path_entry))))
				abort();

			new->pe_prev = work;
			if(work) work->pe_next = new;

			new->pe_next = NULL;
			if (next_slash) {
				int len = next_slash - cpath;
				new->pe_full_path = malloc(len + 1);
				strncpy(new->pe_full_path, cpath, len);
				new->pe_full_path[len] = '\0';
			} else {
				/* no more slashes */
				new->pe_full_path = strdup(cpath);
			}
			new->pe_last_component_name = new->pe_full_path + (start-cpath);
			work = new;
			if(!listp->pl_first) listp->pl_first = work;
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"created entry 0x%X '%s' '%s'",
				(unsigned long int)work, work->pe_full_path, start);
		}
		if (next_slash) start = next_slash + 1;
	} while (next_slash != NULL);

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_entries_to_string(listp->pl_first);

		SB_LOG(SB_LOGLEVEL_NOISE2, "split->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
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
	if (p_entry->pe_full_path) free(p_entry->pe_full_path);
	free(p_entry);
	return(ret);
}

static void remove_last_path_entry(struct path_entry_list *listp)
{
	struct path_entry *work;

	work = listp->pl_first;

	while (work && work->pe_next) {
		work = work->pe_next;
	}
	if (work) {
		/* now "work" points to last element in the list */
		remove_path_entry(listp, work);
	}
}

static void remove_dots_and_dotdots_from_path_entries(
	struct path_entry_list *listp)
{
	struct path_entry *work = listp->pl_first;

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_entries_to_string(listp->pl_first);

		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: Clean  ->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
	while (work) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: work=0x%X examine '%s'",
			(unsigned long int)work, work?work->pe_last_component_name:"");
		if (strcmp(work->pe_last_component_name, "..") == 0) {
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
		} else if (strcmp(work->pe_last_component_name, ".") == 0) {
			/* ignore this node */
			work = remove_path_entry(listp, work);
		} else {
			work = work->pe_next;
		}
	}
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_entries_to_string(listp->pl_first);

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
	split_path_to_path_entries(path, &list);
	remove_dots_and_dotdots_from_path_entries(&list);

	buf = path_entries_to_string(list.pl_first);
	free_path_entries(&list);

	SB_LOG(SB_LOGLEVEL_NOISE, "sb_clean_path returns '%s'", buf);
	return buf;
}

/* dirname() is not thread safe (may return pointer to static buffer),
 * so we'll have our own version. This requires that the parameter
 * is an absolute path, always:
*/
static char *sb_abs_dirname(const char *abs_path)
{
	struct path_entry_list list;
	char *buf = NULL;

	if (!abs_path || (*abs_path != '/')) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"FATAL internal error: sb_abs_dirname called with"
			" illegal parameter (%s)", abs_path);
		assert(0);
	}

	split_path_to_path_entries(abs_path, &list);
	remove_last_path_entry(&list);

	buf = path_entries_to_string(list.pl_first);
	free_path_entries(&list);

	SB_LOG(SB_LOGLEVEL_NOISE, "sb_abs_dirname '%s' => '%s'", abs_path, buf);
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
	int result_log_level,
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *decolon_path,
	int *flagsp)
{
	int flags;
	char *translate_result = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_translate_path for %s(%s)",
		func_name, decolon_path);
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
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, decolon_path);
	 /* 4 arguments, returns rule,policy,path,flags */
	lua_call(luaif->lua, 4, 4);

	translate_result = (char *)lua_tostring(luaif->lua, -2);
	if (translate_result && (*translate_result != '/')) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Mapping failed: Result is not absolute ('%s'->'%s')",
			decolon_path, translate_result);
		translate_result = NULL;
	} else if (translate_result) {
		translate_result = strdup(translate_result);
	}
	flags = lua_tointeger(luaif->lua, -1);
	check_mapping_flags(flags, "sbox_translate_path");
	if (flagsp) *flagsp = flags;
	lua_pop(luaif->lua, 2); /* leave rule and policy to the stack */

	if (translate_result) {
		/* sometimes a mapping rule may create paths that contain
		 * doubled slashes ("//") or end with a slash. We'll
		 * need to clean the path here.
		*/
		char *cleaned_path;

		cleaned_path = sb_clean_path(translate_result);

		if (*cleaned_path != '/') {
			/* oops, got a relative path. CWD is too long. */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"OOPS, call_lua_function_sbox_translate_path:"
				" relative");
		}
		free(translate_result);
		translate_result = NULL;

		/* log the result */
		if (strcmp(cleaned_path, decolon_path) == 0) {
			/* NOTE: Following SB_LOG() call is used by the log
			 *       postprocessor script "sb2logz". Do not change
			 *       without making a corresponding change to
			 *       the script!
			*/
			SB_LOG(result_log_level, "pass: %s '%s'%s",
				func_name, decolon_path,
				((flags & SB2_MAPPING_RULE_FLAGS_READONLY) ? " (readonly)" : ""));
		} else {
			/* NOTE: Following SB_LOG() call is used by the log
			 *       postprocessor script "sb2logz". Do not change
			 *       without making a corresponding change to
			 *       the script!
			*/
			SB_LOG(result_log_level, "mapped: %s '%s' -> '%s'%s",
				func_name, decolon_path, cleaned_path,
				((flags & SB2_MAPPING_RULE_FLAGS_READONLY) ? " (readonly)" : ""));
		}
		translate_result = cleaned_path;
	}
	if (!translate_result) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"No result from sbox_translate_path for: %s '%s'",
			func_name, decolon_path);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_translate_path: at exit, gettop=%d",
		lua_gettop(luaif->lua));
	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_sbox_translate_path exit",
			luaif->lua);
	}
	return(translate_result);
}

/* - returns 1 if ok (then *min_path_lenp is valid)
 * - returns 0 if failed to find the rule
 * Note: this leave the rule to the stack!
*/
static int call_lua_function_sbox_get_mapping_requirements(
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *full_path_for_rule_selection,
	int *min_path_lenp,
	int *call_translate_for_all_p)
{
	int rule_found;
	int min_path_len;
	int flags;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"calling sbox_get_mapping_requirements for %s(%s)",
		func_name, full_path_for_rule_selection);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_get_mapping_requirements: gettop=%d",
		lua_gettop(luaif->lua));

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX,
		"sbox_get_mapping_requirements");
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, full_path_for_rule_selection);
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

static char *call_lua_function_sbox_reverse_path(
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *full_path)
{
	char *orig_path = NULL;
	int flags;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_reverse_path for %s(%s)",
		func_name, full_path);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_reverse_path");
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, full_path);
	 /* 3 arguments, returns orig_path and flags */
	lua_call(luaif->lua, 3, 2);

	orig_path = (char *)lua_tostring(luaif->lua, -2);
	if (orig_path) {
		orig_path = strdup(orig_path);
	}

	flags = lua_tointeger(luaif->lua, -1);
	check_mapping_flags(flags, "sbox_reverse_path");
	/* Note: "flags" is not yet used for anything, intentionally */
 
	lua_pop(luaif->lua, 2); /* remove return values */

	if (orig_path) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "orig_path='%s'", orig_path);
	} else {
		SB_LOG(SB_LOGLEVEL_INFO,
			"No result from sbox_reverse_path for: %s '%s'",
			func_name, full_path);
	}
	return(orig_path);
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

/* sb_path_resolution():  This is the place where symlinks are followed.
 *
 * Returns an allocated buffer containing the resolved path (or NULL if error)
 *
 * Note: when this function returns, lua stack contains the rule which was
 *       used to do the path resolution. drop_rule_from_lua_stack() must
 *       be called after it is not needed anymore!
*/
static void sb_path_resolution(
	mapping_results_t *res,
	int nest_count,
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *abs_path,		/* MUST be an absolute path! */
	int dont_resolve_final_symlink)
{
	struct path_entry_list orig_path_list;
	char *buf = NULL;
	struct path_entry *work;
	int	component_index = 0;
	int	min_path_len_to_check;
	char	*decolon_tmp;
	char	*prefix_mapping_result;
	struct path_entry_list prefix_path_list;
	int	prefix_mapping_result_flags;
	int	call_translate_for_all = 0;

	if (nest_count > 16) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Detected too deep nesting "
			"(too many symbolic links, path='%s')",
			abs_path);

		/* return ELOOP to the calling program */
		res->mres_errno = ELOOP;
		return;
	}

	if (!abs_path || !*abs_path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_path_resolution called with NULL path");
		return;
	}
	if (*abs_path != '/') {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"FATAL: sb_path_resolution called with relative path");
		assert(0);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution %d '%s'", nest_count, abs_path);

	orig_path_list.pl_first = NULL;

	split_path_to_path_entries(abs_path, &orig_path_list);

	work = orig_path_list.pl_first;

	if (call_lua_function_sbox_get_mapping_requirements(
		luaif, binary_name, func_name, abs_path,
		&min_path_len_to_check, &call_translate_for_all)) {
		/* has requirements:
		 * skip over path components that we are not supposed to check,
		 * because otherwise rule recognition & execution could fail.
		*/
		while ((int)strlen(work->pe_full_path) < min_path_len_to_check) {
			SB_LOG(SB_LOGLEVEL_NOISE2, "skipping [%d] '%s'",
				component_index, work->pe_last_component_name);
			component_index++;
			work = work->pe_next;
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "Map prefix [%d] '%s'",
		component_index, work->pe_full_path);

	prefix_path_list.pl_first = NULL;
	split_path_to_path_entries(work->pe_full_path, &prefix_path_list);
	remove_dots_and_dotdots_from_path_entries(&prefix_path_list);
	decolon_tmp = path_entries_to_string(prefix_path_list.pl_first);
	free_path_entries(&prefix_path_list);

	SB_LOG(SB_LOGLEVEL_NOISE, "decolon_tmp => %s", decolon_tmp);

	prefix_mapping_result = call_lua_function_sbox_translate_path(
		SB_LOGLEVEL_NOISE,
		luaif, binary_name, "PATH_RESOLUTION",
		decolon_tmp, &prefix_mapping_result_flags);
	free(decolon_tmp);
	drop_policy_from_lua_stack(luaif);

	SB_LOG(SB_LOGLEVEL_NOISE, "prefix_mapping_result before loop => %s",
		prefix_mapping_result);

	/* Path resolution loop = walk thru directories, and if a symlink
	 * is found, recurse..
	*/
	while (work) {
		char	link_dest[PATH_MAX+1];
		int	link_len;

		if (prefix_mapping_result_flags &
		    SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH) {
			/* "force_orig_path" is set when symlinks MUST NOT
			 * be followed. */
			SB_LOG(SB_LOGLEVEL_NOISE,
				"force_orig_path set => path resolution finished");
			break;
		}

		if (dont_resolve_final_symlink && (work->pe_next == NULL)) {
			/* this is last component, but here a final symlink
			 * must not be resolved (calls like lstat(), rename(),
			 * etc)
			*/
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"Won't check last component [%d] '%s'",
				component_index, work->pe_full_path);
			break;
		}

		SB_LOG(SB_LOGLEVEL_NOISE, "path_resolution: test if symlink [%d] '%s'",
			component_index, prefix_mapping_result);

		/* determine if "prefix_mapping_result" is a symbolic link.
		 * this can't be done with lstat(), because lstat() does not
		 * exist as a function on Linux => lstat_nomap() can not be
		 * used eiher. fortunately readlink() is an ordinary function.
		*/
		link_len = readlink_nomap(prefix_mapping_result, link_dest, PATH_MAX);

		if (link_len > 0) {
			/* was a symlink */
			char	*new_path = NULL;
			char	*rest_of_path;

			link_dest[link_len] = '\0';
			if (work->pe_next) {
				rest_of_path = path_entries_to_string(
					work->pe_next);
				SB_LOG(SB_LOGLEVEL_NOISE,
					"symlink:more: rest='%s'", rest_of_path);
			} else {
				/* last component of the path was a symlink. */
				rest_of_path = strdup("");
				SB_LOG(SB_LOGLEVEL_NOISE,
					"symlink:last: rest=''");
			}

			if (*link_dest == '/') {
				/* absolute symlink.
				 * This is easy: just join the symlink
				 * and rest of path, and further mapping
				 * operations will take care of the rest.
				*/
				SB_LOG(SB_LOGLEVEL_NOISE,
					"absolute symlink at '%s' "
					"points to '%s', restarting",
					prefix_mapping_result, link_dest);

				if (*rest_of_path) {
					if (asprintf(&new_path, "%s%s%s",
						link_dest, 
						((last_char_in_str(link_dest) != '/')
						    && (*rest_of_path != '/') ?
							"/" : ""),
						rest_of_path) < 0) {

						SB_LOG(SB_LOGLEVEL_ERROR,
							"asprintf failed");
					}
				} else {
					new_path = strdup(link_dest);
				}
			} else {
				/* A relative symlink. Somewhat complex:
				 * "prefix_mapping_result" contains the
				 * real location in the FS, but here we
				 * must still build the full path from the
				 * place where we pretend to be - otherwise
				 * path mapping code would fail to find the
				 * correct location. Hence "dirnam" is
				 * based on what was mapped, and not based on
				 * were the mapping took us.
				*/
				char *dirnam;
				int last_in_dirnam_is_slash;

				/* work->pe_full_path is an absolute path */
				dirnam = sb_abs_dirname(work->pe_full_path);

				last_in_dirnam_is_slash =
					(last_char_in_str(dirnam) == '/');

				SB_LOG(SB_LOGLEVEL_NOISE,
					"relative symlink at '%s' "
					"points to '%s'",
					prefix_mapping_result, link_dest);

				if (*rest_of_path) {
					int last_in_dest_is_slash =
					    (last_char_in_str(link_dest)=='/');

					if (asprintf(&new_path, "%s%s%s%s%s",
						dirnam,
						(last_in_dirnam_is_slash ?
							"" : "/"),
						link_dest,
						(!last_in_dest_is_slash &&
						 (*rest_of_path != '/') ?
							"/" : ""),
						rest_of_path) < 0) {

						SB_LOG(SB_LOGLEVEL_ERROR,
							"asprintf failed");
					}
				} else {
					if (asprintf(&new_path, "%s%s%s",
						dirnam,
						(last_in_dirnam_is_slash ?
							"" : "/"),
						link_dest) < 0) {

						SB_LOG(SB_LOGLEVEL_ERROR,
							"asprintf failed");
					}
				}
				free(dirnam);
			}
			free(prefix_mapping_result);
			free(rest_of_path);
			free_path_entries(&orig_path_list);

			/* double-check the result. We MUST use absolute
			 * paths here.
			*/
			if (*new_path != '/') {
				/* this should never happen */
				SB_LOG(SB_LOGLEVEL_ERROR,
					"FATAL: symlink resolved to "
					"a relative path (internal error)");
				assert(0);
			}

			/* recursively call myself to perform path
			 * resolution steps for the symlink target.
			*/

			/* First, forget the old rule: */
			drop_rule_from_lua_stack(luaif);

			/* Then the recursion */
			sb_path_resolution(res, nest_count + 1,
				luaif, binary_name, func_name,
				new_path, dont_resolve_final_symlink);

			/* and finally, cleanup */
			free(new_path);
			return;
		}
		work = work->pe_next;
		if (work) {
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
				if (prefix_mapping_result) {
					free(prefix_mapping_result);
				}
				prefix_mapping_result =
					call_lua_function_sbox_translate_path(
						SB_LOGLEVEL_NOISE,
						luaif, binary_name,
						"PATH_RESOLUTION/2",
						work->pe_full_path, &prefix_mapping_result_flags);
				drop_policy_from_lua_stack(luaif);
			} else {
				/* "standard mapping", based on prefix or
				 * exact match. Ok to skip sbox_translate_path()
				* because here it would just add the component
				 * to end of the path; instead we'll do that
				 * here. This is a performance optimization.
				*/
				char	*next_dir = NULL;

				if (asprintf(&next_dir, "%s/%s",
					prefix_mapping_result,
					work->pe_last_component_name) < 0) {
					SB_LOG(SB_LOGLEVEL_ERROR,
						"asprintf failed");
				}
				if (prefix_mapping_result) {
					free(prefix_mapping_result);
				}
				prefix_mapping_result = next_dir;
			}
		} else {
			free(prefix_mapping_result);
		}
		component_index++;
	}

	/* All symbolic links have been resolved.
	 *
	 * Since there are no symlinks in "orig_path_list", "." and ".."
	 * entries can be safely removed:
	*/
	remove_dots_and_dotdots_from_path_entries(&orig_path_list);

	buf = path_entries_to_string(orig_path_list.pl_first);
	free_path_entries(&orig_path_list);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution returns '%s'", buf);
	res->mres_result_buf = res->mres_result_path = buf;
}

/* ========== Mapping & path resolution, internal implementation: ========== */

/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function.
 * Returns a pointer to an allocated buffer which contains the result.
 */
static void sbox_map_path_internal(
	const char *binary_name,
	const char *func_name,
	const char *orig_path,
	int dont_resolve_final_symlink,
	int process_path_for_exec,
	mapping_results_t *res)
{
	struct lua_instance *luaif = NULL;
	char *mapping_result = NULL;
	const char *abs_path = NULL;
	char *abs_path_buffer = NULL;
	char real_cwd[PATH_MAX + 1]; /* used only if orig_path is relative */

	SB_LOG(SB_LOGLEVEL_DEBUG, "sbox_map_path_internal: %s(%s)", func_name, orig_path);

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
	if (!orig_path || !*orig_path) {
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
			func_name, orig_path);
		goto use_orig_path_as_result_and_exit;
	}

	luaif = get_lua();
	if (!luaif) {
		/* init in progress? */
		goto use_orig_path_as_result_and_exit;
	}
	if (luaif->mapping_disabled) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "disabled(%d): %s '%s'",
			luaif->mapping_disabled, func_name, orig_path);
		goto use_orig_path_as_result_and_exit;
	}

	/* Going to map it. The mapping logic must get absolute paths: */
	if (*orig_path == '/') {
		abs_path = orig_path;
	} else {
		/* convert to absolute path. */
		char *reversed_cwd = NULL;

		if (!getcwd_nomap_nolog(real_cwd, sizeof(real_cwd))) {
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
			goto use_orig_path_as_result_and_exit;
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: converting to abs.path cwd=%s",
			real_cwd);
		
		/* reversing of paths is expensive...try if a previous
		 * result can be used, and call the reversing logic only if
		 * CWD has been changed.
		*/
		if (luaif->real_cwd && luaif->reversed_cwd &&
		    !strcmp(real_cwd, luaif->real_cwd)) {
			/* "cache hit" */
			reversed_cwd = luaif->reversed_cwd;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: using cached rev_cwd=%s",
				reversed_cwd);
		} else {
			/* "cache miss" */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: reversing cwd:");
			reversed_cwd = call_lua_function_sbox_reverse_path(
				luaif,
				(sbox_binary_name?sbox_binary_name:"UNKNOWN"),
				func_name, real_cwd);
			if (reversed_cwd == NULL) {
				/*
				 * In case reverse path couldn't be resolved
				 * we fallback into real_cwd.  This is the
				 * way it used to work before.
				 */
				reversed_cwd = strdup(real_cwd);
				SB_LOG(SB_LOGLEVEL_DEBUG,
				    "unable to reverse, using reversed_cwd=%s",
				    reversed_cwd);
			}
			/* put the reversed CWD to our one-slot cache: */
			if (luaif->real_cwd) free(luaif->real_cwd);
			if (luaif->reversed_cwd) free(luaif->reversed_cwd);
			luaif->real_cwd = strdup(real_cwd);
			luaif->reversed_cwd = reversed_cwd;
		}
		if (asprintf(&abs_path_buffer, "%s/%s", reversed_cwd, orig_path) < 0) {
			/* asprintf failed */
			abort();
		}
		abs_path = abs_path_buffer;
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: abs.path is '%s'",
			abs_path);
	}

	disable_mapping(luaif);
	{
		/* Mapping disabled inside this block - do not use "return"!! */
		mapping_results_t	decolon_path_result;
		char			*full_path_for_rule_selection = NULL;

		clear_mapping_results_struct(&decolon_path_result);

		full_path_for_rule_selection = sb_clean_path(abs_path);

		if (!full_path_for_rule_selection ||
		    (*full_path_for_rule_selection != '/')) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"sbox_map_path_internal: sb_clean_path()"
				" failed to return absolute path (can't"
				" map this)");
			mapping_result = strdup(abs_path);
			if (process_path_for_exec) {
				/* can't map, but still need to leave "rule"
				 * (string) and "policy" (nil) to the stack */
				lua_pushstring(luaif->lua,
					"mapping failed (failed to make it absolute)");
				lua_pushnil(luaif->lua);
			}
			goto forget_mapping;
		}

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sbox_map_path_internal: process '%s', n='%s'",
			orig_path, full_path_for_rule_selection);

		/* sb_path_resolution() leaves the rule to the stack... */
		sb_path_resolution(&decolon_path_result, 0,
			luaif, binary_name, func_name,
			full_path_for_rule_selection,
			dont_resolve_final_symlink);

		if (decolon_path_result.mres_errno) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"path mapping fails, "
				" errno = %d",
				decolon_path_result.mres_errno);
			res->mres_errno = decolon_path_result.mres_errno;
			goto forget_mapping;
		}

		if (!decolon_path_result.mres_result_path) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"sbox_map_path_internal: "
				"decolon_path failed [%s]",
				func_name);
			mapping_result = NULL;
			if (process_path_for_exec) {
				/* can't map, but still need to leave "rule"
				 * (string) and "policy" (nil) to the stack */
				lua_pushstring(luaif->lua, 
					"mapping failed (decolon path failed)");
				lua_pushnil(luaif->lua);
			}
		} else {
			int	flags;

			SB_LOG(SB_LOGLEVEL_NOISE2,
				"sbox_map_path_internal: decolon_path='%s'",
				decolon_path_result.mres_result_path);

			mapping_result = call_lua_function_sbox_translate_path(
				SB_LOGLEVEL_INFO,
				luaif, binary_name, func_name,
				decolon_path_result.mres_result_path, &flags);
			res->mres_readonly = (flags & SB2_MAPPING_RULE_FLAGS_READONLY);

			if (process_path_for_exec == 0) {
				/* ...and remove rule and policy from stack */
				drop_policy_from_lua_stack(luaif);
				drop_rule_from_lua_stack(luaif);
			}
		}
	forget_mapping:
		free_mapping_results(&decolon_path_result);
		if(full_path_for_rule_selection) free(full_path_for_rule_selection);
	}
	enable_mapping(luaif);

	res->mres_result_buf = res->mres_result_path = mapping_result;

	/* now "mapping_result" is an absolute path.
	 * sb2's exec logic needs absolute paths, and absolute paths are also
	 * needed when registering paths to the "fdpathdb".  but otherwise,
	 * we'll try to return a relative path if the original path was
	 * relative (abs.path is still available in mres_result_buf).
	*/
	if ((process_path_for_exec == 0) &&
	    (orig_path[0] != '/') &&
	    mapping_result &&
	    (*mapping_result == '/')) {
		/* orig_path was relative. real_cwd has been filled above */
		int	real_cwd_len = strlen(real_cwd);
		int	result_len = strlen(mapping_result);

		if ((result_len == real_cwd_len) &&
		    !strcmp(real_cwd, mapping_result)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: result==CWD");
			res->mres_result_path = strdup(".");
			res->mres_result_path_was_allocated = 1;
		} else if ((result_len > real_cwd_len) &&
			   (mapping_result[real_cwd_len] == '/') &&
			   (mapping_result[real_cwd_len+1] != '/') &&
			   (mapping_result[real_cwd_len+1] != '\0') &&
			   !strncmp(real_cwd, mapping_result, real_cwd_len)) {
			/* real_cwd is a prefix of result; convert result
			 * back to a relative path
			*/
			char *relative_result = mapping_result+real_cwd_len+1;
			res->mres_result_path = relative_result;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"sbox_map_path_internal: result==relative (%s) (%s)",
				relative_result, mapping_result);
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "sbox_map_path_internal: mapping_result='%s'",
		mapping_result ? mapping_result : "<No result>");
	release_lua(luaif);
	if (abs_path_buffer) free(abs_path_buffer);
	return;

    use_orig_path_as_result_and_exit:
	if(luaif) release_lua(luaif);
	res->mres_result_buf = res->mres_result_path = strdup(orig_path);
	return;
}

/* ========== Public interfaces to the mapping & resolution code: ========== */

void sbox_map_path_for_sb2show(
	const char *binary_name,
	const char *func_name,
	const char *path,
	mapping_results_t *res)
{
	if (!path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		sbox_map_path_internal(binary_name, func_name, path,
			0/*dont_resolve_final_symlink*/, 0, res);
	}
}

void sbox_map_path(
	const char *func_name,
	const char *path,
	int dont_resolve_final_symlink,
	mapping_results_t *res)
{
	if (!path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name, path, dont_resolve_final_symlink, 0, res);
	}
}

void sbox_map_path_at(
	const char *func_name,
	int dirfd,
	const char *path,
	int dont_resolve_final_symlink,
	mapping_results_t *res)
{
	const char *dirfd_path;

	if (!path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
		return;
	}

	if ((*path == '/')
#ifdef AT_FDCWD
	    || (dirfd == AT_FDCWD)
#endif
	   ) {
		/* same as sbox_map_path() */
		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			path, dont_resolve_final_symlink, 0, res);
		return;
	}

	/* relative to something else than CWD */
	dirfd_path = fdpathdb_find_path(dirfd);

	if (dirfd_path) {
		/* pathname found */
		char *at_full_path = NULL;

		if (asprintf(&at_full_path, "%s/%s", dirfd_path, path) < 0) {
			/* asprintf failed */
			abort();
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"Synthetic path for %s(%d,'%s') => '%s'",
			func_name, dirfd, path, at_full_path);

		sbox_map_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			at_full_path, dont_resolve_final_symlink, 0, res);
		free(at_full_path);

		return;
	}

	/* name not found. Can't do much here, log a warning and return
	 * the original relative path. That will work if we are lucky, but
	 * not always..  */
	SB_LOG(SB_LOGLEVEL_WARNING, "Path not found for FD %d, for %s(%s)",
		dirfd, func_name, path);
	res->mres_result_buf = res->mres_result_path = strdup(path);
	res->mres_readonly = 0;
}

/* this maps the path and then leaves "rule" and "exec_policy" to the stack, 
 * because exec post-processing needs them
*/
void sbox_map_path_for_exec(
	const char *func_name,
	const char *path,
	mapping_results_t *res)
{
	sbox_map_path_internal(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"), func_name,
		path, 0/*dont_resolve_final_symlink*/, 1/*exec mode*/, res);
}

char *scratchbox_reverse_path(
	const char *func_name,
	const char *full_path)
{
	struct lua_instance *luaif = get_lua();
	char *result;

	result = call_lua_function_sbox_reverse_path(luaif,
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
		func_name, full_path);
	release_lua(luaif);
	return(result);
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

