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

#define __set_errno(e) errno = e

void mapping_log_write(char *msg);
static int lua_bind_sb_functions(lua_State *l);

static pthread_key_t lua_key;
static pthread_once_t lua_key_once = PTHREAD_ONCE_INIT;

static void free_lua(void *buf)
{
	free(buf);
}

static void alloc_lua_key(void)
{
	pthread_key_create(&lua_key, free_lua);
}

static void alloc_lua(void)
{
	struct lua_instance *tmp;

	tmp = malloc(sizeof(struct lua_instance));
	memset(tmp, 0, sizeof(struct lua_instance));
	pthread_once(&lua_key_once, alloc_lua_key);
	pthread_setspecific(lua_key, tmp);
	
	tmp->script_dir = getenv("SBOX_LUA_SCRIPTS");

	if (!tmp->script_dir) {
		tmp->script_dir = "/scratchbox/lua_scripts";
	} else {
		tmp->script_dir = strdup(tmp->script_dir);
	}
		
	tmp->main_lua_script = calloc(strlen(tmp->script_dir)
			+ strlen("/main.lua") + 1, sizeof(char));

	strcpy(tmp->main_lua_script, tmp->script_dir);
	strcat(tmp->main_lua_script, "/main.lua");

	SB_LOG(SB_LOGLEVEL_DEBUG, "script_dir: '%s'",
			tmp->script_dir);

	tmp->lua = luaL_newstate();

	disable_mapping(tmp);
	luaL_openlibs(tmp->lua);
	lua_bind_sb_functions(tmp->lua); /* register our sb_ functions */
	switch(luaL_loadfile(tmp->lua, tmp->main_lua_script)) {
	case LUA_ERRFILE:
		fprintf(stderr, "Error loading %s\n",
				tmp->main_lua_script);
		exit(1);
	case LUA_ERRSYNTAX:
		fprintf(stderr, "Syntax error in %s\n",
				tmp->main_lua_script);
		exit(1);
	case LUA_ERRMEM:
		fprintf(stderr, "Memory allocation error while "
				"loading %s\n", tmp->main_lua_script);
		exit(1);
	default:
		;
	}
	lua_call(tmp->lua, 0, 0);
	enable_mapping(tmp);
	SB_LOG(SB_LOGLEVEL_DEBUG, "lua initialized.");
}

struct lua_instance *get_lua(void)
{
	return (struct lua_instance *)pthread_getspecific(lua_key);
}

int lua_engine_state = 0;

void sb2_lua_init(void) __attribute((constructor));
void sb2_lua_init(void)
{
	lua_engine_state = LES_INIT_IN_PROCESS;
	sblog_init();
	alloc_lua();
	lua_engine_state = LES_READY;
}

/* "sb.decolonize_path", to be called from lua code */
static int lua_sb_decolonize_path(lua_State *l)
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

	resolved_path = sb_decolonize_path(path);
	lua_pushstring(l, resolved_path);
	free(resolved_path);
	free(path);
	return 1;
}

/* "sb.readlink", to be called from lua code */
static int lua_sb_readlink(lua_State *l)
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

/* "sb.getdirlisting", to be called from lua code */
static int lua_sb_getdirlisting(lua_State *l)
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
		lua_pushstring(l, de->d_name); /* push the entries to
						* lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	free(path);
	return 1;
}

/* "sb.log": interface from lua to the logging system.
 * Parameters:
 *  1. log level (string)
 *  2. log message (string)
*/
static int lua_sb_log(lua_State *luastate)
{
	char	*logmsg;
	char	*loglevel;
	int	n = lua_gettop(luastate);

	if (n != 2) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_log_from_lua: wrong number of params (%d)", n);
		lua_pushstring(luastate, NULL);
		return 1;
	}

	/* FIXME: is it necessary to use strdup here? */
	loglevel = strdup(lua_tostring(luastate, 1));
	logmsg = strdup(lua_tostring(luastate, 2));

	if(!strcmp(loglevel, "debug"))
		SB_LOG(SB_LOGLEVEL_DEBUG, ">> %s", logmsg);
	else if(!strcmp(loglevel, "info"))
		SB_LOG(SB_LOGLEVEL_INFO, "INFO: %s", logmsg);
	else if(!strcmp(loglevel, "warning"))
		SB_LOG(SB_LOGLEVEL_WARNING, "WARNING: %s", logmsg);
	else if(!strcmp(loglevel, "error"))
		SB_LOG(SB_LOGLEVEL_ERROR, "ERROR: %s", logmsg);
	else /* default to level "error"  */
		SB_LOG(SB_LOGLEVEL_ERROR, "%s", logmsg);

	free(loglevel);
	free(logmsg);

	lua_pushnumber(luastate, 1);
	return 1;
}


/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"getdirlisting",		lua_sb_getdirlisting},
	{"readlink",			lua_sb_readlink},
	{"decolonize_path",		lua_sb_decolonize_path},
	{"log",				lua_sb_log},
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
