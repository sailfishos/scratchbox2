#include <config.h>

#include <lua.h>
#include <lauxlib.h>


int sb2_global_vars_initialized__  = 0;

void
checknargs(lua_State *L, int maxargs)
{
	int nargs = lua_gettop(L);
	lua_pushfstring(L, "no more than %d argument%s expected, got %d",
		        maxargs, maxargs == 1 ? "" : "s", nargs);
	luaL_argcheck(L, nargs <= maxargs, maxargs + 1, lua_tostring (L, -1));
	lua_pop(L, 1);
}

static int
argtypeerror(lua_State *L, int narg, const char *expected)
{
	const char *got = luaL_typename(L, narg);
	return luaL_argerror(L, narg,
		lua_pushfstring(L, "%s expected, got %s", expected, got));
}

const char*
optstring(lua_State *L, int narg, const char *def)
{
	const char *s;
        if (lua_isnoneornil(L, narg)) {
		if (def) return def;
        }

	s = lua_tolstring(L, 1, NULL);
        if (!s) {
		if (def) {
			argtypeerror(L, narg, "nil or string");
		} else {
			argtypeerror(L, narg, "string");
		}
        }
        return s;
}
