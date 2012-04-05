/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2012 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <unistd.h>
#include <stdint.h>
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

#include "mapping.h"
#include "sb2.h"
#include "rule_tree.h"
#include "sb2_network.h"
#include "sb2_vperm.h"
#include "libsb2.h"
#include "exported.h"


#define __set_errno(e) errno = e

static pthread_key_t sb2context_key;
static pthread_once_t sb2context_key_once = PTHREAD_ONCE_INIT;

static void alloc_sb2context_key(void)
{
	if (pthread_key_create_fnptr)
#if 0
		(*pthread_key_create_fnptr)(&sb2context_key, free_lua);
#else
		/* FIXME: Do we need a destructor? */
		(*pthread_key_create_fnptr)(&sb2context_key, NULL);
#endif
}

/* used only if pthread lib is not available: */
static	struct sb2context *my_sb2context = NULL;

static struct sb2context *alloc_sb2context(void)
{
	struct sb2context *tmp;

	if (pthread_getspecific_fnptr) {
		tmp = (*pthread_getspecific_fnptr)(sb2context_key);
		if (tmp != NULL) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"alloc_sb2context: already done (pt-getspec.)");
			return(tmp);
		}
	} else if (my_sb2context) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"alloc_sb2context: already done (has my_sb2context)");
		return(my_sb2context);
	}

	tmp = malloc(sizeof(struct sb2context));
	if (!tmp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"alloc_sb2context: Failed to allocate memory");
		return(NULL);
	}
	memset(tmp, 0, sizeof(struct sb2context));

	if (pthread_setspecific_fnptr) {
		(*pthread_setspecific_fnptr)(sb2context_key, tmp);
	} else {
		my_sb2context = tmp;
	}
	
	if (!sbox_session_dir || !*sbox_session_dir) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"alloc_sb2context: no SBOX_SESSION_DIR");
		return(NULL); /* can't live without a session */
	}
	sbox_session_dir = strdup(sbox_session_dir);
	return(tmp);
}

static void increment_sb2if_usage_counter(volatile struct sb2context *ptr)
{
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		/* Well, to make this bullet-proof the sb2if structure
		 * should be locked, but since this code is now used only for
		 * producing debugging information and the pointer is marked
		 * "volatile", the results are good enough. No need to slow 
		 * down anything with additional locks - this function is 
		 * called frequently. */
		if (ptr->sb2context_in_use > 0) SB_LOG(SB_LOGLEVEL_DEBUG,
			"Lua instance already in use! (%d)",
			ptr->sb2context_in_use);

		(ptr->sb2context_in_use)++;
	}
}

void release_sb2context(struct sb2context *sb2if)
{
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		int	i;
		volatile struct sb2context *ptr = sb2if;

		SB_LOG(SB_LOGLEVEL_NOISE, "release_sb2context()");

		if (!ptr) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"release_sb2context(): ptr is NULL ");
			return;
		}

		i = ptr->sb2context_in_use;
		if (i > 1) SB_LOG(SB_LOGLEVEL_DEBUG,
			"Lua instance usage counter was %d", i);

		(ptr->sb2context_in_use)--;
	}
}

/* get access to sb2 context, create the structure 
 * if it didn't exist; in that case, the structure is only
 * cleared (most notably, the Lua system is not initialized
 * by this routine!)
 *
 * Remember to call release_sb2context() after the
 * pointer is not needed anymore.
*/
struct sb2context *get_sb2context(void)
{
	struct sb2context *ptr = NULL;

	if (!sb2_global_vars_initialized__) sb2_initialize_global_variables();

	if (!SB_LOG_INITIALIZED()) sblog_init();

	SB_LOG(SB_LOGLEVEL_NOISE, "get_sb2context()");

	if (pthread_detection_done == 0) check_pthread_library();

	if (pthread_library_is_available) {
		if (pthread_once_fnptr)
			(*pthread_once_fnptr)(&sb2context_key_once, alloc_sb2context_key);
		if (pthread_getspecific_fnptr)
			ptr = (*pthread_getspecific_fnptr)(sb2context_key);
		if (!ptr) ptr = alloc_sb2context();
		if (!ptr) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Something's wrong with"
				" the pthreads support");
			fprintf(stderr, "FATAL: sb2 preload library:"
				" Something's wrong with"
				" the pthreads support.\n");
			exit(1);
		}
	} else {
		/* no pthreads, single-thread application */
		ptr = my_sb2context;
		if (!ptr) ptr = alloc_sb2context();
		if (!ptr) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Failed to get Lua instance"
				" (and the pthreads support is "
				" disabled!)");
			fprintf(stderr, "FATAL: sb2 preload library:"
				" Failed to get Lua instance"
				" (and the pthreads support is disabled!)\n");
			exit(1);
		}
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		increment_sb2if_usage_counter(ptr);
	}
	return(ptr);
}

/* Preload library constructor. Unfortunately this can
 * be called after other parts of this library have been called
 * if the program uses multiple threads (unbelievable, but true!),
 * so this isn't really too useful. Lua initialization was
 * moved to get_sb2context_lua() because of this.
*/
#ifndef SB2_TESTER
#ifdef __GNUC__
void sb2_preload_library_constructor(void) __attribute((constructor));
#endif
#endif /* SB2_TESTER */
void sb2_preload_library_constructor(void)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "sb2_preload_library_constructor called");
	sblog_init();
	SB_LOG(SB_LOGLEVEL_DEBUG, "sb2_preload_library_constructor: done");
}

/* Return the library interface version string (used to be Lua/C if.vrs,
 * but it is still needed even if Lua is gone)
 * Note that this function is exported from libsb2.so (for sb2-show etc): */
const char *sb2__lua_c_interface_version__(void)
{
	/* currently it is enough to return pointer to the constant string. */
	return(SB2_LUA_C_INTERFACE_VERSION);
}

int test_if_str_in_colon_separated_list_from_env(
	const char *str, const char *env_var_name)
{
	int	result = 0;	/* boolean; default result is "not found" */
	char	*list;
	char	*tok = NULL;
	char	*tok_state = NULL;

	list = getenv(env_var_name);
	if (!list) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "no %s", env_var_name);
		return(0);
	}
	list = strdup(list);	/* will be modified by strtok_r */
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s is '%s'", env_var_name, list);

	tok = strtok_r(list, ":", &tok_state);
	while (tok) {
		result = !strcmp(str, tok);
		if (result) break; /* return if matched */
		tok = strtok_r(NULL, ":", &tok_state);
	}
	free(list);
	return(result);
}

