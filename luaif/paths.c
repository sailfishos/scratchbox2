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

extern int lua_engine_state;

/* ========== Path & Path component handling primitives: ========== */

struct path_entry {
	struct path_entry *pe_prev;
	struct path_entry *pe_next;
	char *pe_last_component_name;
	char pe_full_path[PATH_MAX];
};

struct path_entry_list {
	struct path_entry *pl_first;
};

static char *path_entries_to_string(char *buf, struct path_entry *work)
{
	*buf = '\0';
	if (!work) {
		/* "work" will be empty if orig.path was "/." */
		strcat(buf, "/");
	} else while (work) {
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
	char tmp_path_buf[PATH_MAX+1];

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
			strcpy(new->pe_full_path, cpath);
			new->pe_last_component_name = new->pe_full_path + (start-cpath);
			work = new;
			if(!listp->pl_first) listp->pl_first = work;
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"created entry 0x%X '%s' '%s'",
				(int)work, work->pe_full_path, start);
		}

		if (last)
			break;
		*index = '/';
		start = index + 1;
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"split->'%s'", path_entries_to_string(tmp_path_buf, listp->pl_first));
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
	struct path_entry *work;
	char tmp_path_buf[PATH_MAX+1];

	work = listp->pl_first;

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"remove_dots_and_dotdots: Clean  ->'%s'",
		path_entries_to_string(tmp_path_buf, listp->pl_first));
	while (work) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"remove_dots_and_dotdots: work=0x%X examine '%s'",
			(int)work, work?work->pe_last_component_name:"");
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
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"remove_dots_and_dotdots: cleaned->'%s'",
		path_entries_to_string(tmp_path_buf, listp->pl_first));
}

static char *absolute_path(const char *path)
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
		if (!getcwd(cwd, sizeof(cwd))) {
			/* getcwd() returns NULL if the path is really long.
			 * In this case this really won't be able to do all 
			 * path mapping steps, but sb_decolonize_path()
			 * must not fail!
			*/
			SB_LOG(SB_LOGLEVEL_ERROR,
				"absolute_path failed to get current dir"
				" (processing continues with relative path)");
			if (!(cpath = strdup(path)))
				abort();
		} else {
			asprintf(&cpath, "%s/%s", cwd, path);
			if (!cpath)
				abort();
		}
		SB_LOG(SB_LOGLEVEL_NOISE, "absolute_path done, '%s'", cpath);
	}
	return(cpath);
}

char *sb_decolonize_path(const char *path)
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

	cpath = absolute_path(path);

	split_path_to_path_entries(cpath, &list);
	remove_dots_and_dotdots_from_path_entries(&list);

	buf = malloc((PATH_MAX + 1) * sizeof(char));
	memset(buf, '\0', PATH_MAX + 1);

	path_entries_to_string(buf, list.pl_first);
	free_path_entries(&list);

	SB_LOG(SB_LOGLEVEL_NOISE, "sb_decolonize_path returns '%s'", buf);
	return buf;
}

/* dirname() is not thread safe (may return pointer to static buffer),
 * so we'll have our own version, which always returns absolute dirnames:
*/
static char *sb_abs_dirname(const char *path)
{
	char *cpath;
	struct path_entry_list list;
	char *buf = NULL;

	if (!path) return strdup(".");
	SB_LOG(SB_LOGLEVEL_NOISE, "sb_abs_dirname '%s'", path);

	list.pl_first = NULL;

	cpath = absolute_path(path);

	split_path_to_path_entries(cpath, &list);
	remove_last_path_entry(&list);

	buf = malloc((PATH_MAX + 1) * sizeof(char));
	memset(buf, '\0', PATH_MAX + 1);

	path_entries_to_string(buf, list.pl_first);
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
 * at exit this leaves the rule to stack.
*/
static char *call_lua_function_sbox_translate_path(
	int result_log_level,
	struct lua_instance *luaif,
	const char *binary_name,
	const char *func_name,
	const char *work_dir,
	const char *decolon_path,
	int *ro_flagp)
{
	int ro_flag;
	char *traslate_result = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_translate_path for %s(%s)",
		func_name, decolon_path);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	/* stack now contains the rule object and string "sbox_translate_path",
         * move the string to the bottom: */
	lua_insert(luaif->lua, -2);
	/* add other parameters */
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, work_dir);
	lua_pushstring(luaif->lua, decolon_path);
	lua_call(luaif->lua, 5, 3); /* 5 arguments, returns rule,path,ro_flag */

	traslate_result = (char *)lua_tostring(luaif->lua, -2);
	if (traslate_result) {
		traslate_result = strdup(traslate_result);
	}
	ro_flag = lua_toboolean(luaif->lua, -1);
	if (ro_flagp) *ro_flagp = ro_flag;
	lua_pop(luaif->lua, 2); /* leave rule to the stack */

	if (traslate_result) {
		/* sometimes a mapping rule may create paths that contain
		 * doubled slashes ("//") or end with a slash. We'll
		 * need to clean the path here.
		*/
		char *cleaned_path;

		cleaned_path = sb_decolonize_path(traslate_result);
		free(traslate_result);
		traslate_result = NULL;

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
		return cleaned_path;
	}
	SB_LOG(SB_LOGLEVEL_ERROR,
		"No result from sbox_translate_path for: %s '%s'",
		func_name, decolon_path);
	return(NULL);
}

/* - returns 1 if ok (then *min_path_lenp is valid)
 * - returns 0 if failed to find the rule
 * Note: this leave the rule to the stack!
*/
static int call_lua_function_sbox_get_mapping_requirements(
	struct lua_instance *luaif,
	const char *mapping_mode,
	const char *binary_name,
	const char *func_name,
	const char *work_dir,
	const char *full_path_for_rule_selection,
	int *min_path_lenp)
{
	int rule_found;
	int min_path_len;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"calling sbox_get_mapping_requirements for %s(%s)",
		func_name, full_path_for_rule_selection);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX,
		"sbox_get_mapping_requirements");
	lua_pushstring(luaif->lua, mapping_mode);
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, work_dir);
	lua_pushstring(luaif->lua, full_path_for_rule_selection);
	/* five arguments, returns (rule, flag, min_path_len) */
	lua_call(luaif->lua, 5, 3);

	rule_found = lua_toboolean(luaif->lua, -2);
	min_path_len = lua_tointeger(luaif->lua, -1);
	if (min_path_lenp) *min_path_lenp = min_path_len;

	/* remove "flag" and "min_path_len"; leave "rule" to the stack */
	lua_pop(luaif->lua, 2);

	SB_LOG(SB_LOGLEVEL_DEBUG, "sbox_get_mapping_requirements -> %d,%d",
		rule_found, min_path_len);

	return(rule_found);
}

/* ========== Path resolution: ========== */

/* clean up path resolution environment from lua stack */
static void drop_rule_from_lua_stack(struct lua_instance *luaif)
{
	/* remove "rule" from the stack.  */
	lua_pop(luaif->lua, 1);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"path resolution cleanup: at exit, gettop=%d",
		lua_gettop(luaif->lua));
}

/* sb_path_resolution():  This is the place where symlinks are followed.
 *
 * Returns an allocated buffer containing the resolved path (or NULL if error)
 *
 * Note: when this function returns, lua stack contains the rule which was
 *       used to do the path resolution. drop_rule_from_lua_stack() must
 *       be called after it is not needed anymore!
 *
 * FIXME: It might be possible to eliminate parameter
 * "full_path_for_rule_selection" now when the separate call to
 * sbox_get_mapping_requirements is used to ensure that "path" is long
 * enough. However, this has been left to be done in the future, together
 * with other optimizations.
*/
static char *sb_path_resolution(
	int nest_count,
	struct lua_instance *luaif,
	const char *mapping_mode,
	const char *binary_name,
	const char *func_name,
	const char *work_dir,
	const char *path,
	const char *full_path_for_rule_selection,
	int dont_resolve_final_symlink)
{
	char *cpath;
	struct path_entry_list orig_path_list;
	char *buf = NULL;
	struct path_entry *work;
	int	component_index = 0;
	int	min_path_len_to_check;

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

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_path_resolution called with NULL path");
		return NULL;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution %d '%s'", nest_count, path);

	orig_path_list.pl_first = NULL;

	cpath = absolute_path(path);

	split_path_to_path_entries(cpath, &orig_path_list);

	work = orig_path_list.pl_first;

	if (call_lua_function_sbox_get_mapping_requirements(
		luaif, mapping_mode, binary_name, func_name,
		work_dir, full_path_for_rule_selection,
		&min_path_len_to_check)) {
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

	/* Path resolution loop = walk thru directories, and if a symlink
	 * is found, recurse..
	*/
	while (work) {
		char	link_dest[PATH_MAX];
		char	decolon_tmp[PATH_MAX];
		int	link_len;
		char	*prefix_mapping_result;
		struct path_entry_list prefix_path_list;
		int	ro_tmp;

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

		SB_LOG(SB_LOGLEVEL_NOISE2, "test [%d] '%s'",
			component_index, work->pe_full_path);

		prefix_path_list.pl_first = NULL;
		split_path_to_path_entries(work->pe_full_path, &prefix_path_list);
		remove_dots_and_dotdots_from_path_entries(&prefix_path_list);
		path_entries_to_string(decolon_tmp, prefix_path_list.pl_first);
		free_path_entries(&prefix_path_list);

		prefix_mapping_result = call_lua_function_sbox_translate_path(
			SB_LOGLEVEL_NOISE,
			luaif, binary_name, "PATH_RESOLUTION",
			work_dir, decolon_tmp, &ro_tmp);

		SB_LOG(SB_LOGLEVEL_NOISE2, "prefix_mapping_result='%s'",
			prefix_mapping_result);

		/* determine if "prefix_mapping_result" is a symbolic link.
		 * this can't be done with lstat(), because lstat() does not
		 * exist as a function on Linux => lstat_nomap() can not be
		 * used eiher. fortunately readlink() is an ordinary function.
		*/
		link_len = readlink_nomap(prefix_mapping_result, link_dest, PATH_MAX);

		if (link_len > 0) {
			/* was a symlink */
			char	new_path[PATH_MAX];
			char	rest_of_path[PATH_MAX];

			link_dest[link_len] = '\0';
			if (work->pe_next) {
				path_entries_to_string(rest_of_path, work->pe_next);
			} else {
				/* last component of the path was a symlink. */
				*rest_of_path = '\0';
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
				strcpy(new_path, link_dest);
				if (*rest_of_path) {
					if ((last_char_in_str(new_path) != '/')
					    && (*rest_of_path != '/')) {
						strcat(new_path, "/");
					}
					strcat(new_path, rest_of_path);
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

				dirnam = sb_abs_dirname(work->pe_full_path);

				SB_LOG(SB_LOGLEVEL_NOISE,
					"relative symlink at '%s' "
					"points to '%s'",
					prefix_mapping_result, link_dest);
				strcpy(new_path, dirnam);
				if (last_char_in_str(new_path) != '/')
					strcat(new_path, "/");
				strcat(new_path, link_dest);
				if (*rest_of_path) {
					if ((last_char_in_str(new_path) != '/')
					    && (*rest_of_path != '/')) {
						strcat(new_path, "/");
					}
					strcat(new_path, rest_of_path);
				}
				free(dirnam);
			}
			free(prefix_mapping_result);
			free_path_entries(&orig_path_list);

			/* recursively call myself to perform path
			 * resolution steps for the symlink target.
			*/

			/* First, forget the old rule: */
			drop_rule_from_lua_stack(luaif);

			/* Then the recursion */
			return(sb_path_resolution(nest_count + 1,
				luaif, mapping_mode, binary_name, func_name,
				work_dir, new_path,
				new_path, dont_resolve_final_symlink));
		}
		free(prefix_mapping_result);
		work = work->pe_next;
		component_index++;
	}

	/* All symbolic links have been resolved.
	 *
	 * Since there are no symlinks in "orig_path_list", "." and ".."
	 * entries can be safely removed:
	*/
	remove_dots_and_dotdots_from_path_entries(&orig_path_list);

	buf = malloc((PATH_MAX + 1) * sizeof(char));
	memset(buf, '\0', PATH_MAX + 1);

	path_entries_to_string(buf, orig_path_list.pl_first);
	free_path_entries(&orig_path_list);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_path_resolution returns '%s'", buf);
	return buf;
}

/* ========== Public interfaces to the mapping & resolution code: ========== */

/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function.
 * Returns a pointer to an allocated buffer which contains the result.
 */
char *scratchbox_path3(const char *binary_name,
		const char *func_name,
		const char *path,
		const char *mapping_mode,
		int *ro_flagp,
		int dont_resolve_final_symlink)
{	
	char work_dir[PATH_MAX + 1];
	struct lua_instance *luaif;
	char *mapping_result;

	SB_LOG(SB_LOGLEVEL_DEBUG, "scratchbox_path3: %s(%s)", func_name, path);

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

	switch (lua_engine_state) {
	case LES_NOT_INITIALIZED:
		sb2_lua_init();
		break;
	case LES_INIT_IN_PROCESS:
		return strdup(path);
	case LES_READY:
	default:
		/* Do nothing */
		break;
	}

	luaif = get_lua();
	if (!luaif) {
		fprintf(stderr, "Something's wrong with"
			" the pthreads support.\n");
		exit(1);
	}

	/* use a sane default for mapping_mode, if not defined by 
	 * the parameter or environment */
	if (!mapping_mode && !(mapping_mode = getenv("SBOX_MAPMODE"))) {
		mapping_mode = "simple";
	}

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: scratchbox_path3: path==NULL [%s]", func_name);
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
		char *full_path_for_rule_selection;

		full_path_for_rule_selection = sb_decolonize_path(path);

		/* FIXME: work_dir should be unnecessary if path is absolute? */
		memset(work_dir, '\0', sizeof(work_dir));
		getcwd(work_dir, sizeof(work_dir)-1);

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"scratchbox_path3: process '%s', n='%s'",
			path, full_path_for_rule_selection);

		/* sb_path_resolution() leaves the rule to the stack... */
		decolon_path = sb_path_resolution(0,
			luaif, mapping_mode, binary_name, func_name,
			work_dir, path, full_path_for_rule_selection,
			dont_resolve_final_symlink);

		if (!decolon_path) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"ERROR: scratchbox_path3: decolon_path failed [%s]",
				func_name);
			mapping_result = NULL;
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"scratchbox_path3: decolon_path='%s'"
				" work_dir='%s'",
				decolon_path, work_dir);

			mapping_result = call_lua_function_sbox_translate_path(
				SB_LOGLEVEL_INFO,
				luaif, binary_name, func_name,
				work_dir, decolon_path, ro_flagp);
		}
		if(decolon_path) free(decolon_path);
		if(full_path_for_rule_selection) free(full_path_for_rule_selection);
		/* ...and remove the rule from stack */
		drop_rule_from_lua_stack(luaif);
	}
	enable_mapping(luaif);

	SB_LOG(SB_LOGLEVEL_NOISE2, "scratchbox_path3: mapping_result='%s'",
		mapping_result ? mapping_result : "<No result>");
	return(mapping_result);
}

char *scratchbox_path(
	const char *func_name,
	const char *path,
	int *ro_flagp,
	int dont_resolve_final_symlink)
{
	char binary_name[PATH_MAX+1];
	char *bin_name;
	const char *mapping_mode;

	memset(binary_name, '\0', PATH_MAX+1);
	if (!(bin_name = getenv("__SB2_BINARYNAME"))) {
		bin_name = "UNKNOWN";
	}
	strcpy(binary_name, bin_name);

	if (!(mapping_mode = getenv("SBOX_MAPMODE"))) {
		mapping_mode = "simple";
	}
	return (scratchbox_path3(binary_name, func_name, path, mapping_mode,
		ro_flagp, dont_resolve_final_symlink));
}

