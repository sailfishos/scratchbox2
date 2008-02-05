/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
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

#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif

extern int lua_engine_state;

struct path_entry {
	struct path_entry *prev;
	struct path_entry *next;
	char name[PATH_MAX];
};

char *sb_decolonize_path(const char *path)
{
	char *cpath, *index, *start;
	char cwd[PATH_MAX + 1];
	struct path_entry list;
	struct path_entry *work;
	struct path_entry *new;
	char *buf = NULL;

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_decolonize_path called with NULL path");
		return NULL;
	}

	buf = malloc((PATH_MAX + 1) * sizeof(char));
	memset(buf, '\0', PATH_MAX + 1);

	list.next = NULL;
	list.prev = NULL;
	work = &list;

	if (path[0] != '/') {
		/* not an absolute path */
		memset(cwd, '\0', sizeof(cwd));
		if (!getcwd(cwd, sizeof(cwd))) {
			/* getcwd() returns NULL if the path is really long.
			 * In this case this really won't be able to do all 
			 * path mapping steps, but sb_decolonize_path()
			 * must not fail!
			*/
			SB_LOG(SB_LOGLEVEL_ERROR,
				"sb_decolonize_path failed to get current dir"
				" (processing continues with relative path)");
			if (!(cpath = strdup(path)))
				abort();
		} else {
			asprintf(&cpath, "%s/%s", cwd, path);
			if (!cpath)
				abort();
		}
	} else {
		if (!(cpath = strdup(path)))
			abort();
	}

	start = cpath;
	while(*start == '/') start++;	/* ignore leading '/' */

	while (1) {
		unsigned int last = 0;

		index = strstr(start, "/");
		if (!index) {
			last = 1;
		} else {
			*index = '\0';
		}

		if (index == start) {
			goto proceed;       /* skip over empty strings 
					       resulting from // */
		}

		if (strcmp(start, "..") == 0) {
			/* travel up one */
			if (!work->prev)
				goto proceed;
			work = work->prev;
			free(work->next);
			work->next = NULL;
		} else if (strcmp(start, ".") == 0) {
			/* ignore */
			goto proceed;
		} else {
			/* add an entry to our path_entry list */
			if (!(new = malloc(sizeof(struct path_entry))))
				abort();
			memset(new->name, '\0', PATH_MAX);
			new->prev = work;
			work->next = new;
			new->next = NULL;
			strcpy(new->name, start);
			work = new;
		}

proceed:
		if (last)
			break;
		*index = '/';
		start = index + 1;
	}

	work = list.next;
	while (work) {
		struct path_entry *tmp;
		strcat(buf, "/");
		strcat(buf, work->name);
		tmp = work;
		work = work->next;
		free(tmp);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_decolonize_path returns '%s'", buf);
	return buf;
}

/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function
 */
char *scratchbox_path3(const char *binary_name,
		const char *func_name,
		const char *path,
		const char *mapping_mode,
		int *ro_flagp)
{	
	char work_dir[PATH_MAX + 1];
	char *tmp = NULL, *decolon_path = NULL;
	char pidlink[17]; /* /proc/2^8/exe */
	struct lua_instance *luaif;
	int ro_flag;

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
	decolon_path = sb_decolonize_path(path);

	if (!decolon_path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: scratchbox_path3: decolon_path failed [%s]",
			func_name);
		return NULL;
	}
	
	memset(work_dir, '\0', PATH_MAX+1);
	snprintf(pidlink, sizeof(pidlink), "/proc/%i/exe", getpid());
	getcwd(work_dir, PATH_MAX);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	lua_pushstring(luaif->lua, mapping_mode);
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, work_dir);
	lua_pushstring(luaif->lua, decolon_path);
	lua_call(luaif->lua, 5, 2); /* five arguments, returns path+ro_flag */

	tmp = (char *)lua_tostring(luaif->lua, -2);
	if (tmp) {
		tmp = strdup(tmp);
	}
	ro_flag = lua_toboolean(luaif->lua, -1);
	if (ro_flagp) *ro_flagp = ro_flag;

	lua_pop(luaif->lua, 2);

	enable_mapping(luaif);

	if (strcmp(tmp, decolon_path) == 0) {
		free(decolon_path);
		free(tmp);
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "pass: %s '%s'%s",
			func_name, path, (ro_flag ? " (readonly)" : ""));
		return strdup(path);
	} else {
		free(decolon_path);
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO, "mapped: %s '%s' -> '%s'%s",
			func_name, path, tmp, (ro_flag ? " (readonly)" : ""));
		return tmp;
	}
}

char *scratchbox_path(const char *func_name, const char *path, int *ro_flagp)
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
		ro_flagp));
}

