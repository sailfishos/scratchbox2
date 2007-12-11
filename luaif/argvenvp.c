/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include <sb2.h>
#include <mapping.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int lua_engine_state;
static char *argvenvp_mode;

int sb_execve_mod(char **file, char ***argv, char ***envp)
{
	struct lua_instance *luaif;
	char **p;
	int i, res, new_argc, new_envc;

	switch (lua_engine_state) {
	case LES_NOT_INITIALIZED:
		sb2_lua_init();
		break;
	case LES_INIT_IN_PROCESS:
		return 0;
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

	if (!(argvenvp_mode = getenv("SBOX_ARGVENVP_MODE"))) {
		argvenvp_mode = "simple";
	}

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_argvenvp: (argv || envp) == NULL");
		return -1;
	}

	if (getenv("SBOX_DISABLE_ARGVENVP")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_argvenvp disabled(E):");
		return 0;
	}

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_execve_mod");
	lua_pushstring(luaif->lua, *file);
	free(*file);

	lua_newtable(luaif->lua);
	for (p = *argv, i = 1; *p; p++, i++) {
		lua_pushnumber(luaif->lua, i);
		lua_pushstring(luaif->lua, *p);
		lua_settable(luaif->lua, -3);
		free(*p);
	}
	free(*argv);

	lua_newtable(luaif->lua);
	for (p = *envp, i = 1; *p; p++, i++) {
		lua_pushnumber(luaif->lua, i);
		lua_pushstring(luaif->lua, *p);
		lua_settable(luaif->lua, -3);
		free(*p);
	}
	free(*envp);

	/* args:    binaryname, argv, envp
	 * returns: err, file, argc, argv, envc, envp */
	lua_call(luaif->lua, 3, 6);
	
	res = lua_tointeger(luaif->lua, -6);
	*file = strdup(lua_tostring(luaif->lua, -5));
	new_argc = lua_tointeger(luaif->lua, -4);
	new_envc = lua_tointeger(luaif->lua, -2);

	*argv = calloc(new_argc + 1, sizeof(char *));
	*envp = calloc(new_envc + 1, sizeof(char *));

	for (i = 0; i < new_argc; i++) {
		lua_rawgeti(luaif->lua, -3, i + 1);
		(*argv)[i] = strdup(lua_tostring(luaif->lua, -1));
		lua_pop(luaif->lua, 1); /* return stack state to what it
					 * was before lua_rawgeti() */
	}
	(*argv)[i] = NULL;

	for (i = 0; i < new_envc; i++) {
		lua_rawgeti(luaif->lua, -1, i + 1);
		(*envp)[i] = strdup(lua_tostring(luaif->lua, -1));
		lua_pop(luaif->lua, 1); /* return stack state to what it
					 * was before lua_rawgeti() */
	}
	(*envp)[i] = NULL;

	return res;
}
