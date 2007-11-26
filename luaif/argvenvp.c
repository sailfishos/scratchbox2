/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <stdlib.h>

#include <sb2.h>
#include <mapping.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int lua_engine_state;
static char *argvenvp_mode;

int sb_argvenvp(const char *binary_name, const char *func_name,
		char **argv, char **envp)
{
	struct lua_instance *luaif;

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
		return 1;
	}

	if (getenv("SBOX_DISABLE_ARGVENVP")) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "sb_argvenvp disabled(E):");
		return 0;
	}

	return 0;
}
