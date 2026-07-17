#ifndef LUA_HELPERS_H
#define LUA_HELPERS_H

#include <lua.h>

void checknargs(lua_State *L, int maxargs);
const char* optstring(lua_State *L, int narg, const char *def);

#endif
