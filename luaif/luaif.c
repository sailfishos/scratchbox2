/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <string.h>
#include <libgen.h>
#define _GNU_SOURCE
#else
#include <string.h>
#include <libgen.h>
#endif

#include <limits.h>
#include <sys/param.h>
#include <sys/file.h>
#include <assert.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <mapping.h>
#include <sb2.h>

/* ------------ WARNING WARNING WARNING ------------
 * A SERIOUS WARNING ABOUT THE "pthread" LIBRARY:
 * The libpthread.so library interferes with some other libraries on
 * Linux. It is enough to just load the library to memory. For example,
 * /usr/bin/html2text on debian "etch" crashes if libpthread.so is loaded,
 * but works fine if libpthread is not loaded (the crash is caused by
 * a segfault inside libstdc++)
 *
 * Because of this, a special interface to the pthread library must be used:
 * We must use the run-time interface to dynamic linker and detect if the
 * pthread library has already been loaded (most likely, by the real program
 * that we are serving).
*/
#include <pthread.h>
#include <dlfcn.h>

/* pointers to pthread library functions, if the pthread library is in use.
*/
static int (*pthread_key_create_fnptr)(pthread_key_t *key,
	 void (*destructor)(void*)) = NULL;
static void *(*pthread_getspecific_fnptr)(pthread_key_t key) = NULL;
static int (*pthread_setspecific_fnptr)(pthread_key_t key,
	const void *value) = NULL;
static int (*pthread_once_fnptr)(pthread_once_t *, void (*)(void)) = NULL;

static void check_pthread_library()
{
	static int pthread_detection_done = 0;

	if (pthread_detection_done == 0) {
		pthread_key_create_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_key_create");
		pthread_getspecific_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_getspecific");
		pthread_setspecific_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_setspecific");
		pthread_once_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_once");

		if (pthread_key_create_fnptr &&
		    pthread_getspecific_fnptr &&
		    pthread_setspecific_fnptr &&
		    pthread_once_fnptr) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"pthread library FOUND");
		} else if (!pthread_key_create_fnptr &&
		   !pthread_getspecific_fnptr &&
		   !pthread_setspecific_fnptr &&
		   !pthread_once_fnptr) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"pthread library not found");
		} else {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"pthread library is only partially available"
				" - operation may become unstable");
		}

		pthread_detection_done = 1;
	}
}

/* ------------ End Of pthreads Warnings & Interface Code ------------ */

#define __set_errno(e) errno = e

void mapping_log_write(char *msg);
static int lua_bind_sb_functions(lua_State *l);

static pthread_key_t lua_key;
static pthread_once_t lua_key_once = PTHREAD_ONCE_INIT;

static char *read_string_variable_from_lua(
	struct lua_instance *luaif,
	const char *name)
{
	char *result = NULL;

	if (luaif && name && *name) {
		lua_getglobal(luaif->lua, name);
		result = (char *)lua_tostring(luaif->lua, -1);
		if (result) {
			result = strdup(result);
		}
		lua_pop(luaif->lua, 1);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"Lua variable %s = '%s', gettop=%d",
			name, (result ? result : "<NULL>"),
			lua_gettop(luaif->lua));
	}
	return(result);
}

static void free_lua(void *buf)
{
	free(buf);
}

static void alloc_lua_key(void)
{
	if (pthread_key_create_fnptr)
		(*pthread_key_create_fnptr)(&lua_key, free_lua);
}

/* used only if pthread lib is not available: */
static	struct lua_instance *my_lua_instance = NULL;

/* used to store session directory & ld setting: 
 * set up as early as possible, so that if the application clears our 
 * environment we'll still know the original values.
*/
char *sbox_session_dir = NULL;
char *sbox_orig_ld_preload = NULL;
char *sbox_orig_ld_library_path = NULL;

static void alloc_lua(void)
{
	struct lua_instance *tmp;
	char *main_lua_script = NULL;
	char *lua_if_version = NULL;

	check_pthread_library();

	if (pthread_once_fnptr)
		(*pthread_once_fnptr)(&lua_key_once, alloc_lua_key);

	if (pthread_getspecific_fnptr) {
		if ((*pthread_getspecific_fnptr)(lua_key) != NULL) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"alloc_lua: already done (pt-getspec.)");
			return;
		}
	} else if (my_lua_instance) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"alloc_lua: already done (has my_lua_instance)");
		return;
	}

	tmp = malloc(sizeof(struct lua_instance));
	memset(tmp, 0, sizeof(struct lua_instance));

	if (pthread_setspecific_fnptr) {
		(*pthread_setspecific_fnptr)(lua_key, tmp);
	} else {
		my_lua_instance = tmp;
	}
	
	if (!sbox_session_dir)
		sbox_session_dir = getenv("SBOX_SESSION_DIR");
	if (!sbox_orig_ld_preload)
		sbox_orig_ld_preload = getenv("LD_PRELOAD");
	if (!sbox_orig_ld_library_path)
		sbox_orig_ld_library_path = getenv("LD_LIBRARY_PATH");

	if (!sbox_session_dir || !*sbox_session_dir) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"alloc_lua: no SBOX_SESSION_DIR");
		return; /* can't live without a session */
	}
	sbox_session_dir = strdup(sbox_session_dir);

	asprintf(&main_lua_script, "%s/lua_scripts/main.lua", sbox_session_dir);
		
	SB_LOG(SB_LOGLEVEL_DEBUG, "Loading '%s'", main_lua_script);

	tmp->lua = luaL_newstate();

	disable_mapping(tmp);
	luaL_openlibs(tmp->lua);
	lua_bind_sb_functions(tmp->lua); /* register our sb_ functions */
	switch(luaL_loadfile(tmp->lua, main_lua_script)) {
	case LUA_ERRFILE:
		fprintf(stderr, "Error loading %s\n", main_lua_script);
		exit(1);
	case LUA_ERRSYNTAX:
		fprintf(stderr, "Syntax error in %s\n", main_lua_script);
		exit(1);
	case LUA_ERRMEM:
		fprintf(stderr, "Memory allocation error while "
				"loading %s\n", main_lua_script);
		exit(1);
	default:
		;
	}
	lua_call(tmp->lua, 0, 0);
	enable_mapping(tmp);

	/* check Lua/C interface version. */
	lua_if_version = read_string_variable_from_lua(tmp,
		"sb2_lua_c_interface_version");
	if (!lua_if_version) {
		SB_LOG(SB_LOGLEVEL_ERROR, "FATAL ERROR: "
			"sb2's Lua scripts didn't provide"
			" 'sb2_lua_c_interface_version' identifier!");
		exit(1);
	}
	if (strcmp(lua_if_version, SB2_LUA_C_INTERFACE_VERSION)) {
		SB_LOG(SB_LOGLEVEL_ERROR, "FATAL ERROR: "
			"sb2's Lua script interface version mismatch:"
			" scripts provide '%s', but '%s' was expected",
			lua_if_version, SB2_LUA_C_INTERFACE_VERSION);
		exit(1);
	}
	free(lua_if_version);

	SB_LOG(SB_LOGLEVEL_DEBUG, "lua initialized.");
	SB_LOG(SB_LOGLEVEL_NOISE, "gettop=%d", lua_gettop(tmp->lua));

	free(main_lua_script);
}

enum lua_engine_states {
	LES_NOT_INITIALIZED = 0,
	LES_INIT_IN_PROCESS,
	LES_READY
};

static int lua_engine_state = 0;

struct lua_instance *get_lua(void)
{
	struct lua_instance *ptr;

	switch (lua_engine_state) {
	case LES_NOT_INITIALIZED:
		sb2_lua_init();
		break;
	case LES_INIT_IN_PROCESS:
		/* FIXME: This should probably wait.. */
		return NULL;
	case LES_READY:
	default:
		/* Do nothing */
		break;
	}

	if (pthread_getspecific_fnptr) {
		ptr = (*pthread_getspecific_fnptr)(lua_key);
		if (!ptr) {
			fprintf(stderr, "Something's wrong with"
				" the pthreads support.\n");
			exit(1);
		}
	} else {
		ptr = my_lua_instance;
		if (!ptr) {
			fprintf(stderr, "Failed to get Lua instance"
				" (and the pthreads support is disabled!)\n");
			exit(1);
		}
	}
	return(ptr);
}

void sb2_lua_init(void) __attribute((constructor));
void sb2_lua_init(void)
{
	lua_engine_state = LES_INIT_IN_PROCESS;
	sblog_init();
	alloc_lua();
	lua_engine_state = LES_READY;
}

/* Read string variables from lua.
 * Note that this function is exported from libsb2.so (for sb2-show etc): */
char *sb2__read_string_variable_from_lua__(const char *name)
{
	struct lua_instance *luaif;

	luaif = get_lua();
	return(read_string_variable_from_lua(luaif, name));
}


/* "sb.decolonize_path", to be called from lua code */
static int lua_sb_decolonize_path(lua_State *l)
{
	int n;
	char *path;
	char *resolved_path;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	resolved_path = sb_decolonize_path(path);
	lua_pushstring(l, resolved_path);
	free(resolved_path);
	free(path);
	return 1;
}

/* "sb.readlink", to be called from lua code */
static int lua_sb_readlink(lua_State *l)
{
	int n;
	char *path;
	char resolved_path[PATH_MAX + 1];

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	memset(resolved_path, '\0', PATH_MAX + 1);

	path = strdup(lua_tostring(l, 1));
	if (readlink(path, resolved_path, PATH_MAX) < 0) {
		free(path);
		lua_pushstring(l, NULL);
		return 1;
	} else {
		free(path);
		lua_pushstring(l, resolved_path);
		return 1;
	}
}

#if 0 /* Not used anymore. */
/* "sb.getdirlisting", to be called from lua code */
static int lua_sb_getdirlisting(lua_State *l)
{
	DIR *d;
	struct dirent *de;
	char *path;
	int count;

	int n = lua_gettop(l);
	
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	if ( (d = opendir(path)) == NULL ) {
		lua_pushstring(l, NULL);
		return 1;
	}
	count = 1; /* Lua indexes tables from 1 */
	lua_newtable(l); /* create a new table on the stack */
	
	while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
		lua_pushnumber(l, count);
		lua_pushstring(l, de->d_name); /* push the entries to
						* lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	free(path);
	return 1;
}
#endif

/* "sb.log": interface from lua to the logging system.
 * Parameters:
 *  1. log level (string)
 *  2. log message (string)
*/
static int lua_sb_log(lua_State *luastate)
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
	else if(!strcmp(loglevel, "notice"))
		SB_LOG(SB_LOGLEVEL_NOTICE, "NOTICE: %s", logmsg);
	else if(!strcmp(loglevel, "error"))
		SB_LOG(SB_LOGLEVEL_ERROR, "ERROR: %s", logmsg);
	else if(!strcmp(loglevel, "noise"))
		SB_LOG(SB_LOGLEVEL_NOISE, ">>>>: %s", logmsg);
	else if(!strcmp(loglevel, "noise2"))
		SB_LOG(SB_LOGLEVEL_NOISE2, ">>>>>>: %s", logmsg);
	else /* default to level "error"  */
		SB_LOG(SB_LOGLEVEL_ERROR, "%s", logmsg);

	free(loglevel);
	free(logmsg);

	lua_pushnumber(luastate, 1);
	return 1;
}

static int lua_sb_setenv(lua_State *luastate)
{
	int	n = lua_gettop(luastate);

	if (n != 2) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"sb_log_from_lua: wrong number of params (%d)", n);
		lua_pushstring(luastate, NULL);
		return 1;
	}
	setenv(strdup(lua_tostring(luastate, 1)), strdup(lua_tostring(luastate, 2)), 1);
	return 1;
}

/* "sb.path_exists", to be called from lua code
 * returns true if file or directory exists at the specified real path,
 * false if not.
*/
static int lua_sb_path_exists(lua_State *l)
{
	int n;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushboolean(l, 0);
	} else {
		char	*path = strdup(lua_tostring(l, 1));
		int	result = 0;
		SB_LOG(SB_LOGLEVEL_DEBUG, "lua_sb_path_exists testing '%s'",
			path);
		if (access_nomap_nolog(path, F_OK) == 0) {
			/* target exists */
			lua_pushboolean(l, 1);
			result=1;
		} else {
			lua_pushboolean(l, 0);
			result=0;
		}
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
static int lua_sb_debug_messages_enabled(lua_State *l)
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
#if 0
	{"getdirlisting",		lua_sb_getdirlisting},
#endif
	{"readlink",			lua_sb_readlink},
	{"decolonize_path",		lua_sb_decolonize_path},
	{"log",				lua_sb_log},
	{"setenv",			lua_sb_setenv},
	{"path_exists",			lua_sb_path_exists},
	{"debug_messages_enabled",	lua_sb_debug_messages_enabled},
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
