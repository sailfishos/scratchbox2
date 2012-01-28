/*
 * Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 * Portion Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Interfaces to sblib functions from Lua.
*/

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "mapping.h"
#include "sb2.h"
#include "sb2_network.h"
#include "libsb2.h"
#include "exported.h"

/* "sb.log": interface from lua to the logging system.
 * Parameters:
 *  1. log level (string)
 *  2. log message (string)
*/
int lua_sb_log(lua_State *luastate)
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
	else if(!strcmp(loglevel, "network"))
		SB_LOG(SB_LOGLEVEL_NETWORK, "NETWORK: %s", logmsg);
	else if(!strcmp(loglevel, "notice"))
		SB_LOG(SB_LOGLEVEL_NOTICE, "NOTICE: %s", logmsg);
	else if(!strcmp(loglevel, "error"))
		SB_LOG(SB_LOGLEVEL_ERROR, "ERROR: %s", logmsg);
	else if(!strcmp(loglevel, "noise"))
		SB_LOG(SB_LOGLEVEL_NOISE, ">>>>: %s", logmsg);
	else if(!strcmp(loglevel, "noise2"))
		SB_LOG(SB_LOGLEVEL_NOISE2, ">>>>>>: %s", logmsg);
	else if(!strcmp(loglevel, "noise3"))
		SB_LOG(SB_LOGLEVEL_NOISE3, ">>>>>>>>: %s", logmsg);
	else /* default to level "error"  */
		SB_LOG(SB_LOGLEVEL_ERROR, "%s", logmsg);

	free(loglevel);
	free(logmsg);

	lua_pushnumber(luastate, 1);
	return 1;
}

/* "sb.path_exists", to be called from lua code
 * returns true if file, directory or symlink exists at the specified real path,
 * false if not.
*/
int lua_sb_path_exists(lua_State *l)
{
	int n;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushboolean(l, 0);
	} else {
		char	*path = strdup(lua_tostring(l, 1));
		int	result = sb_path_exists(path);

		lua_pushboolean(l, result);
		SB_LOG(SB_LOGLEVEL_DEBUG, "lua_sb_path_exists got %d",
			result);
		free(path);
	}
	return 1;
}

/* "sb.debug_messages_enabled", to be called from lua code
 * returns true if SB_LOG messages have been enabled for the debug levels
 * (debug,noise,noise2...)
*/
int lua_sb_debug_messages_enabled(lua_State *l)
{
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		lua_pushboolean(l, 1);
	} else {
		lua_pushboolean(l, 0);
	}
	return 1;
}

/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"log",				lua_sb_log},
	{"path_exists",			lua_sb_path_exists},
	{"debug_messages_enabled",	lua_sb_debug_messages_enabled},
	{NULL,				NULL}
};

int lua_bind_sblib_functions(lua_State *l)
{
	luaL_register(l, "sblib", reg);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);

	return 0;
}
