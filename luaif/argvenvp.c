/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (exec postprocessing code implemented by Lauri T. Aarnio at Nokia)
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

#include "processclock.h"

/* This stack dump routine is based on an example from the
 * book "Programming in Lua"
 *
 * - This uses logging level DEBUG, but the calls are usually
 *   enabled only at NOISE3.
*/
void dump_lua_stack(const char *msg, lua_State *L)
{
	int i;
	int top = lua_gettop(L);

	SB_LOG(SB_LOGLEVEL_DEBUG, "Stack dump/%s (gettop=%d):", msg, top);

	for (i = 1; i <= top; i++) {
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING: /* strings */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%d: '%s'", i, lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%d: %s", i,
				(lua_toboolean(L, i) ? "true" : "false"));
			break;

		case LUA_TNUMBER:  /* numbers */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%d: %g", i, lua_tonumber(L, i));
			break;

		default:
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%d: %s", i, lua_typename(L, t));
			break;
		}
	}
}

#if 0
/* Convert a vector of strings to a lua table, leaves that table to
 * lua's stack.
*/
static void strvec_to_lua_table(struct sb2context *sb2ctx, char **args)
{
	char	**p;
	int	i;

	lua_newtable(sb2ctx->lua);
	SB_LOG(SB_LOGLEVEL_NOISE2, "strvec_to_lua_table: ");
	for (p = args, i = 1; p && *p; p++, i++) {
		SB_LOG(SB_LOGLEVEL_NOISE2, "set element %d to '%s'", i, *p);
		lua_pushnumber(sb2ctx->lua, i);
		lua_pushstring(sb2ctx->lua, *p);
		lua_settable(sb2ctx->lua, -3);
	}
}
#endif

void strvec_free(char **args)
{
	char **p;

	for (p = args; *p; p++) {
		free(*p);
	}
	free(args);
}

#if 0
/* convert a lua table (table of strings) to a string vector,
 * the vector will be dynamically allocated.
*/
void lua_string_table_to_strvec(lua_State *l,
	int lua_stack_offs, char ***args, int new_argc)
{
	int	i;

	*args = calloc(new_argc + 1, sizeof(char *));

	for (i = 0; i < new_argc; i++) {
		lua_rawgeti(l, lua_stack_offs, i + 1);
		(*args)[i] = strdup(lua_tostring(l, -1));
		lua_pop(l, 1); /* return stack state to what it
					 * was before lua_rawgeti() */
	}
	(*args)[i] = NULL;
}
#endif

#if 0
void sb_push_string_to_lua_stack(char *str) 
{
	struct sb2context *sb2ctx = get_sb2context_lua();

	if (sb2ctx) {
		lua_pushstring(sb2ctx->lua, str);
		release_sb2context(sb2ctx);
	}
}
#endif

#if 0
/* Exec Postprocessing:
*/
int sb_execve_postprocess(const char *exec_type, 
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
	char ***argv,
	char ***envp)
{
	struct sb2context *sb2ctx;
	int res, new_argc;
	int replace_environment = 0;
	PROCESSCLOCK(clk1)

	START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "sb_execve_postprocess");
	sb2ctx = get_sb2context_lua();
	if (!sb2ctx) return(0);

	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("sb_execve_postprocess entry", sb2ctx->lua);
	}

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_argvenvp: (argv || envp) == NULL");
		release_sb2context(sb2ctx);
		return -1;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_postprocess: gettop=%d", lua_gettop(sb2ctx->lua));

	lua_getfield(sb2ctx->lua, LUA_GLOBALSINDEX, "sb_execve_postprocess");

	lua_pushstring(sb2ctx->lua, exec_policy_name);
	lua_pushstring(sb2ctx->lua, exec_type);
	lua_pushstring(sb2ctx->lua, *mapped_file);
	lua_pushstring(sb2ctx->lua, *filename);
	lua_pushstring(sb2ctx->lua, binary_name);
	strvec_to_lua_table(sb2ctx, *argv);
	strvec_to_lua_table(sb2ctx, *envp);

	/* args: exec_policy_name, exec_type, mapped_file, filename,
	 *	 binaryname, argv, envp
	 * returns: res, mapped_file, filename, argc, argv, envc, envp */
	lua_call(sb2ctx->lua, 7, 7);
	
	res = lua_tointeger(sb2ctx->lua, -7);
	switch (res) {

	case 0:
		/* exec arguments were modified, replace contents of
		 * argv vector */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_postprocess: Updated argv&envp");
		free(*mapped_file);
		*mapped_file = strdup(lua_tostring(sb2ctx->lua, -6));

		free(*filename);
		*filename = strdup(lua_tostring(sb2ctx->lua, -5));

		strvec_free(*argv);
		new_argc = lua_tointeger(sb2ctx->lua, -4);
		lua_string_table_to_strvec(sb2ctx->lua, -3, argv, new_argc);

		replace_environment = 1;
		break;

	case 1:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_postprocess: argv was not modified");
		/* always update environment when we are going to exec */
		replace_environment = 1;
		break;

	case -1:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_postprocess: exec denied");
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_execve_postprocess: Unsupported result %d", res);
		break;
	}

	if (replace_environment) {
		int new_envc;
		new_envc = lua_tointeger(sb2ctx->lua, -2);
		strvec_free(*envp);
		lua_string_table_to_strvec(sb2ctx->lua, -1, envp, new_envc);
	}

	/* remove sb_execve_postprocess return values from the stack.  */
	lua_pop(sb2ctx->lua, 7);

	STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, mapped_file);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_postprocess: at exit, gettop=%d", lua_gettop(sb2ctx->lua));
	release_sb2context(sb2ctx);
	return res;
}
#endif

