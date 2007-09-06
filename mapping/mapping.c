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

#define WRITE_LOG(fmt...) \
	{char *__logfile = getenv("SBOX_MAPPING_LOGFILE"); \
	int __logfd; FILE *__logfs;\
	if (__logfile) { \
		if ((__logfd = open(__logfile, O_APPEND | O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) > 0) { \
			__logfs = fdopen(__logfd, "a"); \
			fprintf(__logfs, fmt); \
			fclose(__logfs); \
		} \
	}}


#define enable_mapping(m) m->mapping_disabled--
#define disable_mapping(m) m->mapping_disabled++

#define __set_errno(e) errno = e


void mapping_log_write(char *msg);
static int lua_bind_sb_functions(lua_State *l);

struct mapping {
	lua_State *lua;
	char *script_dir;
	char *main_lua_script;
	int mapping_disabled;
	pthread_mutex_t lua_lock;
};

static pthread_key_t mapping_key;
static pthread_once_t mapping_key_once = PTHREAD_ONCE_INIT;

static void free_mapping(void *buf)
{
	free(buf);
}

static void alloc_mapping_key(void)
{
	pthread_key_create(&mapping_key, free_mapping);
}

void alloc_mapping(void)
{
	struct mapping *tmp;
	tmp = malloc(sizeof(struct mapping));
	memset(tmp, 0, sizeof(struct mapping));
	pthread_once(&mapping_key_once, alloc_mapping_key);
	pthread_setspecific(mapping_key, tmp);
}

struct mapping *get_mapping(void)
{
	return (struct mapping *)pthread_getspecific(mapping_key);
}

static char *dummy = NULL;

void sb2_mapping_init(void) __attribute((constructor));
void sb2_mapping_init(void)
{
	alloc_mapping();
	dummy = "ok";
}

struct path_entry {
	struct path_entry *prev;
	struct path_entry *next;
	char name[PATH_MAX];
};

char *decolonize_path(const char *path)
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


static int sb_decolonize_path(lua_State *l)
{
	int n;
	char *path;
	char *resolved_path;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	resolved_path = decolonize_path(path);
	lua_pushstring(l, resolved_path);
	free(resolved_path);
	free(path);
	return 1;
}

static int sb_readlink(lua_State *l)
{
	int n;
	char *path;
	char resolved_path[PATH_MAX + 1];

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	memset(resolved_path, '\0', PATH_MAX + 1);

	path = strdup(lua_tostring(l, 1));
	if (readlink(path, resolved_path, PATH_MAX) < 0) {
		free(path);
		lua_pushstring(l, NULL);
		return 1;
	} else {
		free(path);
		lua_pushstring(l, resolved_path);
		return 1;
	}
}


static int sb_getdirlisting(lua_State *l)
{
	DIR *d;
	struct dirent *de;
	char *path;
	int count;

	int n = lua_gettop(l);
	
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	if ( (d = opendir(path)) == NULL ) {
		lua_pushstring(l, NULL);
		return 1;
	}
	count = 1; /* Lua indexes tables from 1 */
	lua_newtable(l); /* create a new table on the stack */
	
	while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
		lua_pushnumber(l, count);
		lua_pushstring(l, de->d_name); /* push the entries to lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	free(path);
	return 1;
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
 * to prevent recursive calls to this function */
char *scratchbox_path2(const char *binary_name,
		const char *func_name,
		const char *path)
{	
	char work_dir[PATH_MAX + 1];
	char *tmp = NULL, *decolon_path = NULL, *mapping_mode = NULL;
	char pidlink[17]; /* /proc/2^8/exe */
	struct mapping *m;

	if (!dummy) sb2_mapping_init();

	m = get_mapping();
	if (!m) {
		printf("Something's wrong with"
			" the pthreads support.\n");
		exit(1);
	}

	if (!(mapping_mode = getenv("SBOX_MAPMODE"))) {
		mapping_mode = "simple";
	}

	if (!path) {
		WRITE_LOG("ERROR: scratchbox_path2: path == NULL: [%s][%s]\n",
			binary_name, func_name);
		return NULL;
	}

	if (pthread_mutex_trylock(&m->lua_lock) < 0) {
		pthread_mutex_lock(&m->lua_lock);
	}

	if (m->mapping_disabled || getenv("SBOX_DISABLE_MAPPING")) {
		pthread_mutex_unlock(&m->lua_lock);
		return strdup(path);
	}

	disable_mapping(m);
	decolon_path = decolonize_path(path);

	if (!m->script_dir) {
		m->script_dir = getenv("SBOX_REDIR_SCRIPTS");
		if (!m->script_dir) {
			m->script_dir = "/scratchbox/redir_scripts";
		} else {
			m->script_dir = strdup(m->script_dir);
		}
		
		m->main_lua_script = calloc(strlen(m->script_dir)
				+ strlen("/main.lua") + 1, sizeof(char));

		strcpy(m->main_lua_script, m->script_dir);
		strcat(m->main_lua_script, "/main.lua");
	}
	
	memset(work_dir, '\0', PATH_MAX+1);
	snprintf(pidlink,16, "/proc/%i/exe", getpid());
	getcwd(work_dir, PATH_MAX);

	/* redir_scripts RECURSIVE CALL BREAK */
	if (strncmp(decolon_path, m->script_dir,
			strlen(m->script_dir)) == 0) {
		enable_mapping(m);
		pthread_mutex_unlock(&m->lua_lock);
		return (char *)decolon_path;
	}
	
	if (!m->lua) {
		m->lua = luaL_newstate();
		
		luaL_openlibs(m->lua);
		lua_bind_sb_functions(m->lua); /* register our sb_ functions */
		switch(luaL_loadfile(m->lua, m->main_lua_script)) {
		case LUA_ERRFILE:
			fprintf(stderr, "Error loading %s\n",
					m->main_lua_script);
			exit(1);
		case LUA_ERRSYNTAX:
			fprintf(stderr, "Syntax error in %s\n",
					m->main_lua_script);
			exit(1);
		case LUA_ERRMEM:
			fprintf(stderr, "Memory allocation error while "
					"loading %s\n", m->main_lua_script);
			exit(1);
		default:
			;
		}
		lua_call(m->lua, 0, 0);
	}

	lua_getfield(m->lua, LUA_GLOBALSINDEX, "sbox_translate_path");
	lua_pushstring(m->lua, mapping_mode);
	lua_pushstring(m->lua, binary_name);
	lua_pushstring(m->lua, func_name);
	lua_pushstring(m->lua, work_dir);
	lua_pushstring(m->lua, decolon_path);
	lua_call(m->lua, 5, 1); /* five arguments, one result */

	tmp = (char *)lua_tostring(m->lua, -1);
	if (tmp) {
		tmp = strdup(tmp);
	}

	lua_pop(m->lua, 1);

	enable_mapping(m);

	if (strcmp(tmp, decolon_path) == 0) {
		free(decolon_path);
		free(tmp);
		pthread_mutex_unlock(&m->lua_lock);
		return strdup(path);
	} else {
		free(decolon_path);
		pthread_mutex_unlock(&m->lua_lock);
		return tmp;
	}
}


/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"sb_getdirlisting",		sb_getdirlisting},
	{"sb_readlink",			sb_readlink},
	{"sb_decolonize_path",		sb_decolonize_path},
	{NULL,				NULL}
};


static int lua_bind_sb_functions(lua_State *l)
{
	luaL_register(l, "sb", reg);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);

	return 0;
}
