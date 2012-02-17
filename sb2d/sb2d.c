/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* sb2d, the rule tree server:
 * The one and only process allowed to add data to the rule tree db
 * during an active SB2 session.
 *
 * Other processes may contact this server via a socket (AF_UNIX)
 * and ask the rule tree to be updated; reading from it is allowed
 * without locking, so care must be taken.
 *
 * This implementation assumes that writing 32-bit ints to the
 * memory-mapped file is an atomic operation.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <assert.h>

#include "sb2_server.h"

/* globals */

const char *progname = NULL;
char    *sbox_session_dir = NULL;
char    *pid_file = NULL;


static void write_pid_to_file(pid_t s_pid, const char *pid_file)
{
	if (pid_file) {
		FILE *pidf = NULL;
		pidf = fopen(pid_file, "w");
		if (!pidf) {
			fprintf(stderr, "%s: Failed to open %s for writing\n",
				progname, pid_file);
		} else {
			fprintf(pidf, "%u\n", s_pid);
			fclose(pidf);
		}
	}
}

lua_State *sb2d_lua;

static void load_and_execute_lua_file(const char *filename)
{
	const char *errmsg;

	switch(luaL_loadfile(sb2d_lua, filename)) {
	case LUA_ERRFILE:
		fprintf(stderr, "Error loading %s\n", filename);
		exit(1);
	case LUA_ERRSYNTAX:
		errmsg = lua_tostring(sb2d_lua, -1);
		fprintf(stderr, "Syntax error in %s (%s)\n", filename, 
			(errmsg?errmsg:""));
		exit(1);
	case LUA_ERRMEM:
		fprintf(stderr, "Memory allocation error while "
				"loading %s\n", filename);
		exit(1);
	default:
		;
	}
	lua_call(sb2d_lua, 0, 0);
}

/* Lua calls this at panic: */
static int sb2_lua_panic(lua_State *l)
{
	fprintf(stderr,
		"sb2d: Lua interpreter PANIC: unprotected error in call to Lua API (%s)\n",
		lua_tostring(l, -1));
#if 0
	sblog_init(); /* make sure the logger has been initialized */
#endif
	SB_LOG(SB_LOGLEVEL_ERROR,
		"Lua interpreter PANIC: unprotected error in call to Lua API (%s)\n",
		lua_tostring(l, -1));
	return 0;
}

static void initialize_lua(void)
{
	char *main_lua_script = NULL;

	if (asprintf(&main_lua_script, "%s/lua_scripts/init.lua",
	     sbox_session_dir) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"sb2context_initialize_lua: asprintf failed to allocate memory");
		return;
	}
		
	SB_LOG(SB_LOGLEVEL_INFO, "Loading '%s'", main_lua_script);

	sb2d_lua = luaL_newstate();
	lua_atpanic(sb2d_lua, sb2_lua_panic);

	luaL_openlibs(sb2d_lua);
#if 0
	lua_bind_sb_functions(sb2d_lua); /* register our sb_ functions */
#endif
	lua_bind_ruletree_functions(sb2d_lua); /* register our ruletree_ functions */
	lua_bind_sblib_functions(sb2d_lua); /* register our sblib.* functions */

	load_and_execute_lua_file(main_lua_script);

#if 0
	/* check Lua/C interface version. */
	lua_if_version = read_string_variable_from_lua(sb2ctx,
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
#endif

	SB_LOG(SB_LOGLEVEL_INFO, "lua initialized.");
	SB_LOG(SB_LOGLEVEL_NOISE, "gettop=%d", lua_gettop(sb2d_lua));

	free(main_lua_script);
}

static long parse_num(const char *cp)
{
	long	l;
	char	*end = NULL;

	if (!cp || !*cp) {
		fprintf(stderr, "ERROR: A numeric argument is required");
		exit(1);
	}
	l = strtol(cp, &end, 0);
	if (!end || (end == cp) || (*end != '\0')) {
		fprintf(stderr, "ERROR: %s is not a valid numeric argument", cp);
		exit(1);
	}
	return(l);
}

int main(int argc, char *argv[])
{
	int	opt;
	int	start_server = 1;
	int	backgroud_server = 1;
	char	*debug_level = NULL;
	char	*debug_file = NULL;
	char	*rule_tree_path = NULL;
	uint32_t max_size = 16*1024*1024; /* default 16MB */
	uint32_t min_mmap_addr = 0;
	int	min_client_socket_fd = 279;

	progname = argv[0];

	/* check data types: the rule tree DB uses plain types for
	 * certain data types; these assertions guarantee
	 * that SB2 startup fails if the OS is changed someday
	 * in the future, and data would not fit anumore to the DB...
	*/
	assert(sizeof(uint64_t) >= sizeof(dev_t));
	assert(sizeof(uint64_t) >= sizeof(ino_t));
	assert(sizeof(uint32_t) >= sizeof(uid_t));
	assert(sizeof(uint32_t) >= sizeof(gid_t));
	assert(sizeof(uint32_t) >= sizeof(mode_t));

	while ((opt = getopt(argc, argv, "d:l:s:p:nfS:M:F:")) != -1) {
		switch (opt) {
		case 'd':
			debug_level = strdup(optarg);
			break;
		case 'l':
			debug_file = strdup(optarg);
			break;
		case 's':
			sbox_session_dir = strdup(optarg);
			break;
		case 'p':
			pid_file = strdup(optarg);
			break;
		case 'n': /* don't start server */
			start_server = 0;
			break;
		case 'f': /* foreground server (don't fork) */
			backgroud_server = 0;
			break;
		case 'S':
			max_size = parse_num(optarg);
			break;
		case 'M':
			min_mmap_addr = parse_num(optarg);
			break;
		case 'F':
			min_client_socket_fd = parse_num(optarg);
			break;
		default:
			fprintf(stderr, "Illegal option\n");
			exit(1);
		}
	}
	/* init logging before doing anything else,
	 * otherwise the logger will auto-initialize itself.
	 * if debug_level and/or debug_file is NULL, logger
	 * will read the values from env.vars. */
	sblog_init_level_logfile_format(debug_level, debug_file, NULL);

	if (!sbox_session_dir) {
		fprintf(stderr, "ERROR: "
			"Option '-s session_dir' is mandatory.\n");
		exit(1);
	}

	/* ----- Initialization ----- */

	SB_LOG(SB_LOGLEVEL_DEBUG, "Initializing");

	if (asprintf(&rule_tree_path, "%s/RuleTree.bin", sbox_session_dir) < 0) {
		exit(1);
	}

	if (create_ruletree_file(rule_tree_path,
		max_size, min_mmap_addr, min_client_socket_fd) < 0) {

		SB_LOG(SB_LOGLEVEL_ERROR, "Failed to create rule tree file (%s)",
			rule_tree_path);
		fprintf(stderr, "Failed to create rule DB!\n");
		exit(1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "Rule tree file opened & mapped to memory");

	initialize_lua();

	/* ----- Server ----- */
	if (start_server) {
		pid_t worker_pid;

		/* Write PID to a file */
		if (backgroud_server) {
			if ((worker_pid = fork()) != 0) {
				/* parent */
				SB_LOG(SB_LOGLEVEL_DEBUG, "Parent process");
				write_pid_to_file(worker_pid, pid_file);
				return(0);
			}
		} else {
			write_pid_to_file(getpid(), pid_file);
		}
		
		SB_LOG(SB_LOGLEVEL_DEBUG, "Initializing server");
		create_server_socket();

		/* enter the server loop. 
		 * ruletree_server() returns when the socket has been
		 * deleted and it is time to shut down. */
		ruletree_server();
	}
	return(0);
}

