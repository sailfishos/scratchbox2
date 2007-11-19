/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#define _GNU_SOURCE

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

#define enable_mapping(a) a->mapping_disabled--
#define disable_mapping(a) a->mapping_disabled++

extern char *dummy;

struct path_entry {
	struct path_entry *prev;
	struct path_entry *next;
	char name[PATH_MAX];
};

char *sb_decolonize_path(const char *path)
{
	char *cpath, *index, *start;
	char cwd[PATH_MAX];
	struct path_entry list;
	struct path_entry *work;
	struct path_entry *new;
	char *buf = NULL;

	if (!path) {
		return NULL;
	}

	buf = malloc((PATH_MAX + 1) * sizeof(char));
	memset(buf, '\0', PATH_MAX + 1);

	list.next = NULL;
	list.prev = NULL;
	work = &list;

	if (path[0] != '/') {
		/* not an absolute path */
		memset(cwd, '\0', PATH_MAX);
		if (!getcwd(cwd, PATH_MAX)) {
			perror("error getting current work dir\n");
			return NULL;
		}
		unsigned int l = (strlen(cwd) + 1 + strlen(path) + 1);
		cpath = malloc((strlen(cwd) + 1
					+ strlen(path) + 1) * sizeof(char));
		if (!cpath)
			abort();

		memset(cpath, '\0', l);
		strcpy(cpath, cwd);
		strcat(cpath, "/");
		strcat(cpath, path);
	} else {
		if (!(cpath = strdup(path)))
			abort();
	}

	start = cpath + 1;          /* ignore leading '/' */
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
	return buf;
}

char *scratchbox_path(const char *func_name, const char *path)
{
	char binary_name[PATH_MAX+1];
	char *tmp;

	memset(binary_name, '\0', PATH_MAX+1);
	tmp = getenv("__SB2_BINARYNAME");
	if (tmp) {
		strcpy(binary_name, tmp);
	} else {
		strcpy(binary_name, "UNKNOWN");
	}
	return scratchbox_path2(binary_name, func_name, path);
}


/* make sure to use disable_mapping(m); 
 * to prevent recursive calls to this function
 */
char *scratchbox_path2(const char *binary_name,
		const char *func_name,
		const char *path)
{	
	char work_dir[PATH_MAX + 1];
	char *tmp = NULL, *decolon_path = NULL, *mapping_mode = NULL;
	char pidlink[17]; /* /proc/2^8/exe */
	struct lua_instance *luaif;

	if (!dummy) sb2_lua_init();

	luaif = get_lua();
	if (!luaif) {
		printf("Something's wrong with"
			" the pthreads support.\n");
		exit(1);
	}

	if (!(mapping_mode = getenv("SBOX_MAPMODE"))) {
		mapping_mode = "simple";
	}

	if (!path) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: scratchbox_path2: path==NULL [%s]", func_name);
		return NULL;
	}

	if (getenv("SBOX_DISABLE_MAPPING")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "disabled(E): %s '%s'",
			func_name, path);
		return strdup(path);
	}
	if (luaif->mapping_disabled) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "disabled(%d): %s '%s'",
			luaif->mapping_disabled, func_name, path);
		return strdup(path);
	}

	disable_mapping(luaif);
	decolon_path = sb_decolonize_path(path);
	
	memset(work_dir, '\0', PATH_MAX+1);
	snprintf(pidlink, sizeof(pidlink), "/proc/%i/exe", getpid());
	getcwd(work_dir, PATH_MAX);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	lua_pushstring(luaif->lua, mapping_mode);
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, func_name);
	lua_pushstring(luaif->lua, work_dir);
	lua_pushstring(luaif->lua, decolon_path);
	lua_call(luaif->lua, 5, 1); /* five arguments, one result */

	tmp = (char *)lua_tostring(luaif->lua, -1);
	if (tmp) {
		tmp = strdup(tmp);
	}

	lua_pop(luaif->lua, 1);

	enable_mapping(luaif);

	if (strcmp(tmp, decolon_path) == 0) {
		free(decolon_path);
		free(tmp);
		SB_LOG(SB_LOGLEVEL_INFO, "pass: %s '%s'",
			func_name, path);
		return strdup(path);
	} else {
		free(decolon_path);
		SB_LOG(SB_LOGLEVEL_INFO, "mapped: %s '%s' -> '%s'",
			func_name, path, tmp);
		return tmp;
	}
}

