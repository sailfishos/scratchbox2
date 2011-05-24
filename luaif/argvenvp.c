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

void strvec_free(char **args)
{
	char **p;

	for (p = args; *p; p++) {
		free(*p);
	}
	free(args);
}


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

void sb_push_string_to_lua_stack(char *str) 
{
	struct sb2context *sb2ctx = get_sb2context_lua();

	if (sb2ctx) {
		lua_pushstring(sb2ctx->lua, str);
		release_sb2context(sb2ctx);
	}
}

/* Exec preprocessor:
 * (previously known as "sb_execve_mod")
*/
int sb_execve_preprocess(char **file, char ***argv, char ***envp)
{
	struct sb2context *sb2ctx = NULL;
	int res, new_argc, new_envc;

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_argvenvp: (argv || envp) == NULL");
		return -1;
	}

	if (getenv("SBOX_DISABLE_ARGVENVP")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_argvenvp disabled(E):");
		return 0;
	}

	sb2ctx = get_sb2context_lua();
	if (!sb2ctx) return(0);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_preprocess: gettop=%d", lua_gettop(sb2ctx->lua));

	lua_getfield(sb2ctx->lua, LUA_GLOBALSINDEX, "sbox_execve_preprocess");
	lua_pushstring(sb2ctx->lua, *file);
	free(*file);

	strvec_to_lua_table(sb2ctx, *argv);
	strvec_free(*argv);

	strvec_to_lua_table(sb2ctx, *envp);
	strvec_free(*envp);

	/* args:    binaryname, argv, envp
	 * returns: err, file, argc, argv, envc, envp */
	lua_call(sb2ctx->lua, 3, 6);
	
	res = lua_tointeger(sb2ctx->lua, -6);
	*file = strdup(lua_tostring(sb2ctx->lua, -5));
	new_argc = lua_tointeger(sb2ctx->lua, -4);
	new_envc = lua_tointeger(sb2ctx->lua, -2);

	lua_string_table_to_strvec(sb2ctx->lua, -3, argv, new_argc);
	lua_string_table_to_strvec(sb2ctx->lua, -1, envp, new_envc);

	/* remove sbox_execve_preprocess' return values from the stack.  */
	lua_pop(sb2ctx->lua, 6);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_preprocess: at exit, gettop=%d", lua_gettop(sb2ctx->lua));
	release_sb2context(sb2ctx);
	return res;
}

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

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_postprocess: at exit, gettop=%d", lua_gettop(sb2ctx->lua));
	release_sb2context(sb2ctx);
	return res;
}

/* Map script interpreter:
*/
char *sb_execve_map_script_interpreter(
	const char *exec_policy_name,
	const char *interpreter, 
	const char *interp_arg, 
	const char *mapped_script_filename,
	const char *orig_script_filename,
	char ***argv,
	char ***envp,
	char **new_exec_policy_name)
{
	struct sb2context *sb2ctx;
	char *mapped_interpreter;
	int new_argc, new_envc;
	int res;
	char *nep;

	sb2ctx = get_sb2context_lua();
	if (!sb2ctx) return(0);

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_execve_map_script_interpreter: "
			"(argv || envp) == NULL");
		release_sb2context(sb2ctx);
		return NULL;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_map_script_interpreter: gettop=%d"
		" interpreter=%s interp_arg=%s "
		"mapped_script_filename=%s orig_script_filename=%s "
		"exec_policy_name=%s",
		lua_gettop(sb2ctx->lua), interpreter, interp_arg,
		mapped_script_filename, orig_script_filename,
		exec_policy_name ? exec_policy_name : "NULL");

	lua_getfield(sb2ctx->lua, LUA_GLOBALSINDEX,
		"sb_execve_map_script_interpreter");

	lua_pushstring(sb2ctx->lua, exec_policy_name);
	lua_pushstring(sb2ctx->lua, interpreter);
	if (interp_arg) lua_pushstring(sb2ctx->lua, interp_arg);
	else lua_pushnil(sb2ctx->lua);
	lua_pushstring(sb2ctx->lua, mapped_script_filename);
	lua_pushstring(sb2ctx->lua, orig_script_filename);
	strvec_to_lua_table(sb2ctx, *argv);
	strvec_to_lua_table(sb2ctx, *envp);

	/* args: exec_policy_name, interpreter, interp_arg, 
	 *	 mapped_script_filename, orig_script_filename,
	 *	 argv, envp
	 * returns: exec_policy_name, result, mapped_interpreter, #argv, argv,
	 *	#envp, envp
	 * "result" is one of:
	 *  0: argv / envp were modified; mapped_interpreter was set
	 *  1: argv / envp were not modified; mapped_interpreter was set
	 * -1: deny exec.
	*/
	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("sb_execve_map_script_interpreter M1", sb2ctx->lua);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_map_script_interpreter: call lua, gettop=%d",
		lua_gettop(sb2ctx->lua));
	lua_call(sb2ctx->lua, 7, 7);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_map_script_interpreter: return from lua, gettop=%d",
		lua_gettop(sb2ctx->lua));
	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("sb_execve_map_script_interpreter M2", sb2ctx->lua);
	}
	
	mapped_interpreter = (char *)lua_tostring(sb2ctx->lua, -5);
	if (mapped_interpreter) mapped_interpreter = strdup(mapped_interpreter);

	nep = (char *)lua_tostring(sb2ctx->lua, -7);
	if (new_exec_policy_name)
		*new_exec_policy_name = nep ? strdup(nep) : NULL;

	res = lua_tointeger(sb2ctx->lua, -6);
	switch (res) {

	case 0:
		/* exec arguments were modified, replace contents of
		 * argv and envp vectors */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_map_script_interpreter: Updated argv&envp");

		strvec_free(*argv);
		new_argc = lua_tointeger(sb2ctx->lua, -4);
		lua_string_table_to_strvec(sb2ctx->lua, -3, argv, new_argc);

		new_envc = lua_tointeger(sb2ctx->lua, -2);
		strvec_free(*envp);
		lua_string_table_to_strvec(sb2ctx->lua, -1, envp, new_envc);

		/* remove return values from the stack */
		lua_pop(sb2ctx->lua, 7);
		break;

	case 1:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_map_script_interpreter: argv&envp were not modified");
		/* remove return values from the stack */
		lua_pop(sb2ctx->lua, 7);
		break;

	case 2:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_map_script_interpreter: use sbox_map_path_for_exec");
		/* remove return values from the stack  */
		lua_pop(sb2ctx->lua, 7);
		if (mapped_interpreter) free(mapped_interpreter);
		mapped_interpreter = NULL;
		{
			mapping_results_t	mapping_result;

			clear_mapping_results_struct(&mapping_result);
			sbox_map_path_for_exec("script_interp",
				interpreter, &mapping_result);
			if (mapping_result.mres_result_buf) {
				mapped_interpreter =
					strdup(mapping_result.mres_result_buf);
			}
			nep = mapping_result.mres_exec_policy_name;
			if (new_exec_policy_name)
				*new_exec_policy_name = nep ? strdup(nep) : NULL;
			free_mapping_results(&mapping_result);
		}
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_execve_map_script_interpreter: "
			"interpreter=%s mapped_interpreter=%s policy=%s",
			interpreter, mapped_interpreter,
			nep ? nep : "NULL");
		break;

	case -1:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_map_script_interpreter: exec denied");
		/* remove return values from the stack */
		lua_pop(sb2ctx->lua, 7);
		if (mapped_interpreter) free(mapped_interpreter);
		mapped_interpreter = NULL;
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb_execve_map_script_interpreter: Unsupported result %d", res);
		/* remove return values from the stack */
		lua_pop(sb2ctx->lua, 7);
		break;
	}


	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("sb_execve_map_script_interpreter E2", sb2ctx->lua);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_map_script_interpreter: at exit, gettop=%d "
		"*new_exec_policy_name=%s",
		lua_gettop(sb2ctx->lua),
		(new_exec_policy_name && *new_exec_policy_name) ?
			*new_exec_policy_name : "NULL");
	release_sb2context(sb2ctx);
	return mapped_interpreter;
}

/* Get LD_PRELOAD and LD_LIBRARY_PATH that are used for
 * programs running within the "Host" exec policy.
 * Needed for popen(), at least: popen() uses /bin/sh of 
 * the host as a trampoline to start the process.
*/
void sb_get_host_policy_ld_params(char **p_ld_preload, char **p_ld_lib_path)
{
	struct sb2context *sb2ctx;

	if (getenv("SBOX_DISABLE_ARGVENVP")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_argvenvp disabled(E):");
		return;
	}
	sb2ctx = get_sb2context_lua();
	if (!sb2ctx) return;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_get_host_policy_ld_params: gettop=%d", lua_gettop(sb2ctx->lua));

	lua_getfield(sb2ctx->lua, LUA_GLOBALSINDEX, "sbox_get_host_policy_ld_params");

	/* no args,    
	 * returns: ld_preload, ld_library_path */
	lua_call(sb2ctx->lua, 0, 2);
	
	*p_ld_preload = strdup(lua_tostring(sb2ctx->lua, -2));
	*p_ld_lib_path = strdup(lua_tostring(sb2ctx->lua, -1));

	/* remove return values from the stack.  */
	lua_pop(sb2ctx->lua, 2);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_get_host_policy_ld_params: at exit, gettop=%d", lua_gettop(sb2ctx->lua));
	release_sb2context(sb2ctx);
}

