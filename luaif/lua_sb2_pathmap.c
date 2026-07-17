/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Jolla Mobile LTD.
 * Author: Björn Kettunen
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * ----------------
 *
 * Pathmapping Lua module.
*/

#include <config.h>

#include <lua.h>
#include <lauxlib.h>

#include "lua_helpers.h"
#include <sb2.h>

#include <mapping.h>
/* FIXME: Not sure on this. */
char *sbox_binary_name = "[lua: sb.pathmap]";


#define LUA_FUNCTION(lname, fnname, params)                                          \
  static int lname (lua_State *l)

LUA_FUNCTION(lua_sb_pathmap, "pathmap", "path") {

	checknargs(l, 1);
	const char* path = optstring(l, 1, NULL);

	mapping_results_t mapping_result;

	clear_mapping_results_struct(&mapping_result);

	sbox_map_path(__func__, path, 0/*flags*/,
		      &mapping_result, 0/*classmask*/);

        if (mapping_result.mres_result_path) {
		lua_pushstring(l, mapping_result.mres_result_path);
		return 1;
        }

	lua_pushnil(l);
        return 1;
}

LUA_FUNCTION(lua_sb_reverse_path, "reverse-path", "path") {

	checknargs(l, 1);

	const char* path = optstring(l, 1, NULL);

	char* reversed_path = scratchbox_reverse_path(__func__, path, 0 /*classmask*/);
        if (reversed_path) {
		lua_pushstring(l, reversed_path);
		return 1;
        }

        lua_pushnil(l);
	return 1;
}

static const luaL_Reg lua_sb_pathmap_functions[] = {
    {"path", lua_sb_pathmap},
    {"reverse_path", lua_sb_reverse_path},
    {NULL, NULL}};

LUALIB_API int
luaopen_sb_pathmap(lua_State *l)
{
	SB_INITIAlIZE_GLOBAL_VARIABLES;

	luaL_newlib(l, lua_sb_pathmap_functions);
	lua_pushstring(l, "pathmap");
	lua_setfield(l, -2, "version");

	return 1;
}
