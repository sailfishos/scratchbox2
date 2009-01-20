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

/* Convert a vector of strings to a lua table, leaves that table to
 * lua's stack.
*/
static void strvec_to_lua_table(struct lua_instance *luaif, char **args)
{
	char	**p;
	int	i;

	lua_newtable(luaif->lua);
	for (p = args, i = 1; *p; p++, i++) {
		lua_pushnumber(luaif->lua, i);
		lua_pushstring(luaif->lua, *p);
		lua_settable(luaif->lua, -3);
	}
}

static void strvec_free(char **args)
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
static void lua_string_table_to_strvec(struct lua_instance *luaif,
	int lua_stack_offs, char ***args, int new_argc)
{
	int	i;

	*args = calloc(new_argc + 1, sizeof(char *));

	for (i = 0; i < new_argc; i++) {
		lua_rawgeti(luaif->lua, lua_stack_offs, i + 1);
		(*args)[i] = strdup(lua_tostring(luaif->lua, -1));
		lua_pop(luaif->lua, 1); /* return stack state to what it
					 * was before lua_rawgeti() */
	}
	(*args)[i] = NULL;
}

void sb_push_string_to_lua_stack(char *str) 
{
	struct lua_instance *luaif = get_lua();

	if (luaif) {
		lua_pushstring(luaif->lua, str);
		release_lua(luaif);
	}
}

/* Exec preprocessor:
 * (previously known as "sb_execve_mod")
*/
int sb_execve_preprocess(char **file, char ***argv, char ***envp)
{
	struct lua_instance *luaif = get_lua();
	int res, new_argc, new_envc;

	if (!luaif) return(0);

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_argvenvp: (argv || envp) == NULL");
		release_lua(luaif);
		return -1;
	}

	if (getenv("SBOX_DISABLE_ARGVENVP")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_argvenvp disabled(E):");
		release_lua(luaif);
		return 0;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_preprocess: gettop=%d", lua_gettop(luaif->lua));

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_execve_preprocess");
	lua_pushstring(luaif->lua, *file);
	free(*file);

	strvec_to_lua_table(luaif, *argv);
	strvec_free(*argv);

	strvec_to_lua_table(luaif, *envp);
	strvec_free(*envp);

	/* args:    binaryname, argv, envp
	 * returns: err, file, argc, argv, envc, envp */
	lua_call(luaif->lua, 3, 6);
	
	res = lua_tointeger(luaif->lua, -6);
	*file = strdup(lua_tostring(luaif->lua, -5));
	new_argc = lua_tointeger(luaif->lua, -4);
	new_envc = lua_tointeger(luaif->lua, -2);

	lua_string_table_to_strvec(luaif, -3, argv, new_argc);
	lua_string_table_to_strvec(luaif, -1, envp, new_envc);

	/* remove sbox_execve_preprocess' return values from the stack.  */
	lua_pop(luaif->lua, 6);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_preprocess: at exit, gettop=%d", lua_gettop(luaif->lua));
	release_lua(luaif);
	return res;
}

/* Exec Postprocessing:
 * Called with "rule" and "exec_policy" already in lua's stack.
*/
int sb_execve_postprocess(char *exec_type, 
	char **mapped_file,
	char **filename,
	const char *binary_name,
	char ***argv,
	char ***envp)
{
	struct lua_instance *luaif;
	int res, new_argc, new_envc;

	luaif = get_lua();
	if (!luaif) return(0);

	if (!argv || !envp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ERROR: sb_argvenvp: (argv || envp) == NULL");
		release_lua(luaif);
		return -1;
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_postprocess: gettop=%d", lua_gettop(luaif->lua));

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sb_execve_postprocess");

	/* stack now contains "rule", "exec_policy" and "sb_execve_postprocess".	 * move "sb_execve_postprocess" to the bottom : */
	lua_insert(luaif->lua, -3);

	lua_pushstring(luaif->lua, exec_type);
	lua_pushstring(luaif->lua, *mapped_file);
	lua_pushstring(luaif->lua, *filename);
	lua_pushstring(luaif->lua, binary_name);
	strvec_to_lua_table(luaif, *argv);
	strvec_to_lua_table(luaif, *envp);

	/* args: rule, exec_policy, exec_type, mapped_file, filename,
	 *	 binaryname, argv, envp
	 * returns: err, mapped_file, filename, argc, argv, envc, envp */
	lua_call(luaif->lua, 8, 7);
	
	res = lua_tointeger(luaif->lua, -7);
	switch (res) {

	case 0:
		/* exec arguments were modified, replace contents of
		 * argv and envp vectors */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_postprocess: Updated argv&envp");
		free(*mapped_file);
		*mapped_file = strdup(lua_tostring(luaif->lua, -6));

		free(*filename);
		*filename = strdup(lua_tostring(luaif->lua, -5));

		strvec_free(*argv);
		new_argc = lua_tointeger(luaif->lua, -4);
		lua_string_table_to_strvec(luaif, -3, argv, new_argc);

		new_envc = lua_tointeger(luaif->lua, -2);
		strvec_free(*envp);
		lua_string_table_to_strvec(luaif, -1, envp, new_envc);
		break;

	case 1:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_execve_postprocess: argv&envp were not modified");
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

	/* remove sb_execve_postprocess return values from the stack.  */
	lua_pop(luaif->lua, 6);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"sb_execve_postprocess: at exit, gettop=%d", lua_gettop(luaif->lua));
	release_lua(luaif);
	return res;
}

/*
 * Tries to find exec_policy for given real_binary_name and return
 * field 'field_name' as string.  Returned value should be released
 * by calling free(3) after it is not needed anymore.
 *
 * Returns NULL if field or exec_policy was not found.
 */
char *sb_query_exec_policy(const char *field_name, const char *binary_name,
    const char *real_binary_name)
{
	struct lua_instance *luaif;
	char *result = NULL;
	int ret = 0;

	if ((luaif = get_lua()) == NULL)
		return (NULL);

	SB_LOG(SB_LOGLEVEL_DEBUG, "sb_query_exec_policy: binary_name=%s, "
	    "real_binary_name=%s", binary_name, real_binary_name);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sb_find_exec_policy");
	
	lua_pushstring(luaif->lua, binary_name);
	lua_pushstring(luaif->lua, real_binary_name);

	/*
	 * sb_finc_exec_policy(binary_name, real_binary_name)
	 * returns: err, exec_policy (table)
	 */
	lua_call(luaif->lua, 2, 2);

	ret = lua_tonumber(luaif->lua, -2);
	SB_LOG(SB_LOGLEVEL_DEBUG, "sb_query_exec_policy: ret=%d", ret);

	if (ret) {
		const char *field_value;

		/* get string value from index 'field_name' */
		lua_pushstring(luaif->lua, field_name);
		lua_gettable(luaif->lua, -2);
		if ((field_value = lua_tostring(luaif->lua, -1)) != NULL) {
			result = strdup(field_value);
		}
		lua_pop(luaif->lua, 1);
	}

	/* normalize lua stack */
	lua_pop(luaif->lua, 2);

	release_lua(luaif);
	return (result);
}
