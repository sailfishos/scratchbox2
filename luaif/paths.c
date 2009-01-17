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
	char *cpath,	/* path buffer; will be modified */
	struct path_entry_list	*listp)
{
	struct path_entry *work = NULL;
	char *start;

	SB_LOG(SB_LOGLEVEL_NOISE2, "going to split '%s'", cpath);

	listp->pl_first = NULL;

	start = cpath;
	while(*start == '/') start++;	/* ignore leading '/' */

	while (1) {
		unsigned int last = 0;
		char *index;

		index = strstr(start, "/");
		if (!index) {
			last = 1;
		} else {
			*index = '\0';
		}

		/* ignore empty strings resulting from // */
		if (index != start) {
			struct path_entry *new;

			/* add an entry to our path_entry list */
			if (!(new = calloc(1,sizeof(struct path_entry))))
				abort();

			new->pe_prev = work;
			if(work) work->pe_next = new;

			new->pe_next = NULL;
			new->pe_full_path = strdup(cpath);
			new->pe_last_component_name = new->pe_full_path + (start-cpath);
			work = new;
			if(!listp->pl_first) listp->pl_first = work;
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"created entry 0x%X '%s' '%s'",
				(unsigned long int)work, work->pe_full_path, start);
		}

		if (last)
			break;
		*index = '/';
		start = index + 1;
	}
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

static char *sb_reversing_getcwd(const char *fn_name, char *buf, size_t bufsize)
{
	char	*rev_path = NULL;

	if (!getcwd_nomap_nolog(buf, bufsize)) {
		return(NULL);
	}
	
	rev_path = scratchbox_reverse_path(fn_name, buf);

	if (rev_path) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "REV: '%s' => '%s'", buf, rev_path);
		snprintf(buf, bufsize, "%s", rev_path);
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG, "REV failed.");
	}
	free(rev_path);

	return(buf);
}

static char *absolute_path(const char *fn_name, const char *path)
{
	char *cpath = NULL;

	if (path[0] == '/') {
		/* already absolute path */
		if (!(cpath = strdup(path)))
			abort();
	} else {
		/* not an absolute path */
		char cwd[PATH_MAX + 1];

		memset(cwd, '\0', sizeof(cwd));
		if (!sb_reversing_getcwd(fn_name, cwd, sizeof(cwd))) {
			/* getcwd() returns NULL if the path is really long.
			 * In this case this really won't be able to do all 
			 * path mapping steps, but sb_decolonize_path()
			 * must not fail!
			 *
			 * Added 2009-01-16: This actually happens sometimes;
			 * there are configure scripts that find out MAX_PATH 
			 * the hard way. So, if this process has already
			 * logged this error, well suppress further messages.
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
		if ((asprintf(&cpath, "%s/%s", cwd, path) < 0) || !cpath)
			abort();
		SB_LOG(SB_LOGLEVEL_NOISE, "absolute_path done, '%s'", cpath);
	}
	return(cpath);
}

/* returns an allocated buffer containing absolute,
 * decolonized version of "path"
*/
static char *sb_decolonize_path(const char *fn_name, const char *path)
{
	char *cpath;
	struct path_entry_list list;
	char *buf = NULL;

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_decolonize_path called with NULL path");
		return NULL;
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "sb_decolonize_path '%s'", path);

	list.pl_first = NULL;

	cpath = absolute_path(fn_name, path);
	if (!cpath) {
		SB_LOG(SB_LOGLEVEL_NOTICE,
			"sb_decolonize_path forced to use relative path '%s'",
			path);
		buf = strdup(path);
	} else {
		split_path_to_path_entries(cpath, &list);
		remove_dots_and_dotdots_from_path_entries(&list);

		buf = path_entries_to_string(list.pl_first);
		free_path_entries(&list);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "sb_decolonize_path returns '%s'", buf);
	return buf;
}

/* dirname() is not thread safe (may return pointer to static buffer),
 * so we'll have our own version, which always returns absolute dirnames:
*/
static char *sb_abs_dirname(const char *fn_name, const char *path)
{
	char *cpath;
	struct path_entry_list list;
	char *buf = NULL;

	if (!path) return strdup(".");
	SB_LOG(SB_LOGLEVEL_NOISE, "sb_abs_dirname '%s'", path);

	list.pl_first = NULL;

	cpath = absolute_path(fn_name, path);
	if (!cpath) return(NULL);

	split_path_to_path_entries(cpath, &list);
	remove_last_path_entry(&list);

	buf = path_entries_to_string(list.pl_first);
	free_path_entries(&list);

	SB_LOG(SB_LOGLEVEL_NOISE, "sb_abs_dirname returns '%s'", buf);
	return buf;
}

/* ========== Other helper functions: ========== */

static char last_char_in_str(const char *str)
{
	if (!str || !*str) return('\0');
	while (str[1]) str++;
	return(*str);
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
	int *ro_flagp)
{
	int ro_flag;
	char *translate_result = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_translate_path for %s(%s)",
		func_name, decolon_path);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_translate_path: gettop=%d",
		lua_gettop(luaif->lua));

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	/* stack now contains the rule object and string "sbox_translate_path",
         * move the string to the bottom: */
	lua_insert(luaif->lua, -2);
	/* add other parameters */
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, decolon_path);
	 /* 4 arguments, returns rule,policy,path,ro_flag */
	lua_call(luaif->lua, 4, 4);

	translate_result = (char *)lua_tostring(luaif->lua, -2);
	if (translate_result) {
		translate_result = strdup(translate_result);
	}
	ro_flag = lua_toboolean(luaif->lua, -1);
	if (ro_flagp) *ro_flagp = ro_flag;
	lua_pop(luaif->lua, 2); /* leave rule and policy to the stack */

	if (translate_result) {
		/* sometimes a mapping rule may create paths that contain
		 * doubled slashes ("//") or end with a slash. We'll
		 * need to clean the path here.
		*/
		char *cleaned_path;

		cleaned_path = sb_decolonize_path(func_name, translate_result);
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
				(ro_flag ? " (readonly)" : ""));
		} else {
			/* NOTE: Following SB_LOG() call is used by the log
			 *       postprocessor script "sb2logz". Do not change
			 *       without making a corresponding change to
			 *       the script!
			*/
			SB_LOG(result_log_level, "mapped: %s '%s' -> '%s'%s",
				func_name, decolon_path, cleaned_path,
				(ro_flag ? " (readonly)" : ""));
		}
		SB_LOG(SB_LOGLEVEL_NOISE,
			"call_lua_function_sbox_translate_path: at exit, gettop=%d",
			lua_gettop(luaif->lua));
		return cleaned_path;
	}
	SB_LOG(SB_LOGLEVEL_ERROR,
		"No result from sbox_translate_path for: %s '%s'",
		func_name, decolon_path);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"call_lua_function_sbox_translate_path: at exit, gettop=%d",
		lua_gettop(luaif->lua));
	return(NULL);
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
	int call_translate_for_all;

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
	 * min_path_len, call_translate_for_all) */
	lua_call(luaif->lua, 3, 4);

	rule_found = lua_toboolean(luaif->lua, -3);
	min_path_len = lua_tointeger(luaif->lua, -2);
	call_translate_for_all = lua_toboolean(luaif->lua, -1);
	if (min_path_lenp) *min_path_lenp = min_path_len;
	if (call_translate_for_all_p)
		*call_translate_for_all_p = call_translate_for_all;

	/* remove last 3 values; leave "rule" to the stack */
	lua_pop(luaif->lua, 3);

	SB_LOG(SB_LOGLEVEL_DEBUG, "sbox_get_mapping_requirements -> %d,%d,%d",
		rule_found, min_path_len, call_translate_for_all);

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

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_reverse_path for %s(%s)",
		func_name, full_path);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_reverse_path");
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, full_path);
	 /* 3 arguments, returns orig_path */
	lua_call(luaif->lua, 3, 1);

	orig_path = (char *)lua_tostring(luaif->lua, -1);
	if (orig_path) {
		orig_path = strdup(orig_path);
	}
	lua_pop(luaif->lua, 1); /* remove return value */

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
static char *sb_path_resolution(
	int nest_count,
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *abs_path,
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
	int	ro_tmp;
	char	*path_copy;
	int	call_translate_for_all = 0;

	if (nest_count > 16) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_path_resolution: too deep nesting "
			"(too many symbolic links");

		/* FIXME: This should return ELOOP to the calling program,
		 * but that does not happen currently, because there is no
		 * proper way to signal this kind of failures in the mapping
		 * phase. This is somewhat complex to fix; the fix requires
		 * that the mapping engine interface and other places must
		 * be changed, too (e.g. the interface generator, etc).
		 * This is minor problem currently.
		*/
		errno = ELOOP;
		return NULL;
	}

	if (!abs_path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_path_resolution called with NULL path");
		return NULL;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution %d '%s'", nest_count, abs_path);

	orig_path_list.pl_first = NULL;

	path_copy = strdup(abs_path);
	split_path_to_path_entries(path_copy, &orig_path_list);
	free(path_copy);

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
		decolon_tmp, &ro_tmp);
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
			char	*result_path;

			link_dest[link_len] = '\0';
			if (work->pe_next) {
				rest_of_path = path_entries_to_string(
					work->pe_next);
			} else {
				/* last component of the path was a symlink. */
				rest_of_path = strdup("");
			}
			SB_LOG(SB_LOGLEVEL_NOISE,
				"is symlink: rest='%s'", rest_of_path);

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

				dirnam = sb_abs_dirname(func_name, work->pe_full_path);
				if (!dirnam) {
					/* this should not happen.
					 * work->pe_full_path is supposed to
					 * be absolute path.
					*/
					char *cp;
					SB_LOG(SB_LOGLEVEL_ERROR,
						"relative symlink forced to"
						" use relative path '%s'",
						work->pe_full_path);
					dirnam = strdup(work->pe_full_path);
					cp = strrchr(dirnam,'/');
					if (cp) *cp = '\0';
				}
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

			/* recursively call myself to perform path
			 * resolution steps for the symlink target.
			*/

			/* First, forget the old rule: */
			drop_rule_from_lua_stack(luaif);

			/* Then the recursion */
			result_path = sb_path_resolution(nest_count + 1,
				luaif, binary_name, func_name,
				new_path, dont_resolve_final_symlink);

			/* and finally, cleanup */
			free(new_path);
			return(result_path);
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
						work->pe_full_path, &ro_tmp);
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
	return buf;
}

/* ========== Mapping & path resolution, internal implementation: ========== */

/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function.
 * Returns a pointer to an allocated buffer which contains the result.
 */
static char *scratchbox_path_internal(
	const char *binary_name,
	const char *func_name,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink,
	int process_path_for_exec)
{
	struct lua_instance *luaif;
	char *mapping_result;

	SB_LOG(SB_LOGLEVEL_DEBUG, "scratchbox_path_internal: %s(%s)", func_name, path);

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
	if (!path || !*path) {
		/* an empty path shall always remain empty */
		return strdup("");
	}

	luaif = get_lua();
	if (!luaif) {
		/* init in progress? */
		return strdup(path);
	}

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: scratchbox_path_internal: path==NULL [%s]",
			func_name);
		return NULL;
	}

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "disabled(E): %s '%s'",
			func_name, path);
		return strdup(path);
	}
	if (luaif->mapping_disabled) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "disabled(%d): %s '%s'",
			luaif->mapping_disabled, func_name, path);
		return strdup(path);
	}

	disable_mapping(luaif);
	{
		/* Mapping disabled inside this block - do not use "return"!! */
		char *decolon_path = NULL;
		char *full_path_for_rule_selection = NULL;

		full_path_for_rule_selection = sb_decolonize_path(func_name, path);

		if (*full_path_for_rule_selection != '/') {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"scratchbox_path_internal: sb_decolonize_path"
				" failed to return absolute path (can't"
				" map this)");
			mapping_result = strdup(path);
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
			"scratchbox_path_internal: process '%s', n='%s'",
			path, full_path_for_rule_selection);

		/* sb_path_resolution() leaves the rule to the stack... */
		decolon_path = sb_path_resolution(0,
			luaif, binary_name, func_name,
			full_path_for_rule_selection,
			dont_resolve_final_symlink);

		if (!decolon_path) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"scratchbox_path_internal: "
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
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"scratchbox_path_internal: decolon_path='%s'",
				decolon_path);

			mapping_result = call_lua_function_sbox_translate_path(
				SB_LOGLEVEL_INFO,
				luaif, binary_name, func_name,
				decolon_path, ro_flagp);
			/* ...and remove the rule from stack */
			if (process_path_for_exec == 0) {
				drop_policy_from_lua_stack(luaif);
				drop_rule_from_lua_stack(luaif);
			}
		}
	forget_mapping:
		if(decolon_path) free(decolon_path);
		if(full_path_for_rule_selection) free(full_path_for_rule_selection);
	}
	enable_mapping(luaif);

#if defined(path_registration_has_not_yet_been_fixed_so_this_is_disabled)
	/* This piece of code has been disabled temporarily 2009-01-16 / LTA
	 *
	 * (relative paths can not be used before the registration of
	 * paths to fdpathdb.c can always get absolute paths, even if the
	 * mapping result can be relative otherwise...this requires some
	 * refactoring)
	*/

	/* now "mapping_result" is (should be) an absolute path.
	 * sb2's exec logic needs absolute paths, but otherwise,
	 * try to return a relative path if the original path was relative.
	*/
	if ((process_path_for_exec == 0) &&
	    (path[0] != '/') &&
	    mapping_result &&
	    (*mapping_result == '/')) {
		char cwd[PATH_MAX + 1];

		/* here we want the real CWD, not a reversed one: */
		if (getcwd_nomap_nolog(cwd, sizeof(cwd))) {
			int	cwd_len = strlen(cwd);
			int	result_len = strlen(mapping_result);

			if ((result_len == cwd_len) &&
			    !strcmp(cwd, mapping_result)) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"scratchbox_path_internal: result==CWD");
				free(mapping_result);
				mapping_result = strdup(".");
			} else if ((result_len > cwd_len) &&
			           (mapping_result[cwd_len] == '/') &&
			           (mapping_result[cwd_len+1] != '/') &&
			           (mapping_result[cwd_len+1] != '\0') &&
			           !strncmp(cwd, mapping_result, cwd_len)) {
				/* cwd is a prefix of result; convert result
				 * back to a relative path
				*/
				char *relative_result = strdup(mapping_result+cwd_len+1);
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"scratchbox_path_internal: result==relative (%s) (%s)",
					relative_result, mapping_result);
				free(mapping_result);
				mapping_result = relative_result;
			}
		}
	}
#endif

	SB_LOG(SB_LOGLEVEL_NOISE, "scratchbox_path_internal: mapping_result='%s'",
		mapping_result ? mapping_result : "<No result>");
	return(mapping_result);
}

/* ========== Public interfaces to the mapping & resolution code: ========== */

char *scratchbox_path3(
	const char *binary_name,
	const char *func_name,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink)
{
	return(scratchbox_path_internal(binary_name, func_name, path,
		ro_flagp, dont_resolve_final_symlink, 0));
}

char *scratchbox_path(
	const char *func_name,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink)
{
	return (scratchbox_path_internal(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"), func_name,
		path, ro_flagp, dont_resolve_final_symlink, 0));
}

char *scratchbox_path_at(
	const char *func_name,
	int dirfd,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink)
{
	const char *dirfd_path;

	if ((*path == '/')
#ifdef AT_FDCWD
	    || (dirfd == AT_FDCWD)
#endif
	   ) {
		/* same as scratchbox_path() */
		return (scratchbox_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			path, ro_flagp, dont_resolve_final_symlink, 0));
	}

	/* relative to something else than CWD */
	dirfd_path = fdpathdb_find_path(dirfd);

	if (dirfd_path) {
		/* pathname found */
		char *ret;
		char *at_full_path = NULL;

		if (asprintf(&at_full_path, "%s/%s", dirfd_path, path) < 0) {
			/* asprintf failed */
			abort();
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"Synthetic path for %s(%d,'%s') => '%s'",
			func_name, dirfd, path, at_full_path);

		ret = scratchbox_path_internal(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			func_name,
			at_full_path, ro_flagp, dont_resolve_final_symlink, 0);
		free(at_full_path);
		return(ret);
	}

	/* name not found. Can't do much here, log a warning and return
	 * the original relative path. That will work if we are lucky, but
	 * not always..  */
	SB_LOG(SB_LOGLEVEL_WARNING, "Path not found for FD %d, for %s(%s)",
		dirfd, func_name, path);

	return (strdup(path));
}

/* this maps the path and then leaves "rule" and "exec_policy" to the stack, 
 * because exec post-processing needs them
*/
char *scratchbox_path_for_exec(
	const char *func_name,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink)
{
	return (scratchbox_path_internal(
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"), func_name,
		path, ro_flagp, dont_resolve_final_symlink, 1));
}

char *scratchbox_reverse_path(
	const char *func_name,
	const char *full_path)
{
	struct lua_instance *luaif = get_lua();

	return (call_lua_function_sbox_reverse_path(luaif,
		(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
		func_name, full_path));
}

