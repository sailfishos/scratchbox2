/*
 * Copyright (C) 2006 Lauri Leukkunen <lleukkun@cc.hut.fi>
 *
 * Licensed under LGPL version 2, see top level LICENSE file for details.
 */




/*
 * In the path translation functions we must use syscalls directly to 
 * avoid creating recursive loops of function calls due to wrapping
 */


#include <sys/syscall.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lua_bindings.h"


pidfunction *sb_getpid=getpid;

void bind_set_getpid(pidfunction *func) {
	sb_getpid=func;
}


static int lua_bind_sb_functions(lua_State *l);

/* Lua interpreter */
lua_State *l;

char *rsdir = NULL;
char *main_lua = NULL;



/*
 * This function should ONLY look at things from rsdir
 * any other path leads to loops
 */

static int sb_getdirlisting(lua_State *l)
{
	DIR *d;
	struct dirent *de;
	char *path;
	int count;
	
	int n = lua_gettop(l);
	
	if (n != 1) {
		lua_pushstring(l, "sb_getdirlisting(path) - invalid number of parameters");
		return 1;
	}

	path = strdup(lua_tostring(l, 1));
	if (strncmp(path, rsdir, strlen(rsdir)) != 0) {
		/* invalid path used */
		lua_pushstring(l, "sb_getdirlisting - invalid path");
		return 1;
	}
	
	if ( (d = opendir(path)) == NULL ) {
	}
	count = 0;
	lua_newtable(l); /* create a new table on the stack */
	
	while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
		lua_pushnumber(l, count);
		lua_pushstring(l, de->d_name); /* push the entries to lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	/* make sure lua knows about the table size */
	lua_pushliteral(l, "n");
	lua_pushnumber(l, count - 1);
	lua_rawset(l, -3);
	return 1;
}


char *scratchbox_path(const char *func_name, const char *path)
{	
	char binary_name[PATH_MAX+1];
	char *tmp;
	char pidlink[17]; /* /proc/2^8/exe */
	
	if (!rsdir) {
		rsdir = getenv("SBOX_REDIR_SCRIPTS");
		if (!rsdir) {
			rsdir = "/scratchbox/redir_scripts";
		} else {
			rsdir = strdup(rsdir);
		}
		
		main_lua = calloc(strlen(rsdir) + strlen("/main.lua") + 1, sizeof(char));

		strcpy(main_lua, rsdir);
		strcat(main_lua, "/main.lua");
	}
	
	memset(binary_name, '\0', PATH_MAX+1);
	snprintf(pidlink,16,"/proc/%i/exe",sb_getpid());
	syscall(__NR_readlink, pidlink, binary_name, PATH_MAX);


	/* RECURSIVE CALL BREAK */
	if (strncmp(path, rsdir, strlen(rsdir)) == 0) {
		return (char *)path;
	}
	
	if (!l) {
				
		l = luaL_newstate();
		
		luaL_openlibs(l);
		lua_bind_sb_functions(l); /* register our sb_ functions */
		switch(luaL_loadfile(l, main_lua)) {
		case LUA_ERRFILE:
			fprintf(stderr, "Error loading %s\n", main_lua);
			exit(1);
		case LUA_ERRSYNTAX:
			fprintf(stderr, "Syntax error in %s\n", main_lua);
			exit(1);
		case LUA_ERRMEM:
			fprintf(stderr, "Memory allocation error while loading %s\n", main_lua);
			exit(1);
		default:
			;
		}
		
		lua_call(l, 0, 0);
	}


	lua_getfield(l, LUA_GLOBALSINDEX, "sbox_translate_path");
	lua_pushstring(l, binary_name);
	lua_pushstring(l, func_name);
	lua_pushstring(l, path);
	lua_call(l, 3, 1); /* three arguments, one result */
	tmp = strdup(lua_tostring(l, -1));
	lua_pop(l, 1);
	
	return tmp;
}


/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"sb_getdirlisting",		sb_getdirlisting},
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
