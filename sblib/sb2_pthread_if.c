/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
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

#include <dlfcn.h>

int pthread_library_is_available = 0; /* flag */
int pthread_detection_done = 0;
/* pointers to pthread library functions, if the pthread library is in use.
*/
int (*pthread_key_create_fnptr)(pthread_key_t *key,
	 void (*destructor)(void*)) = NULL;
void *(*pthread_getspecific_fnptr)(pthread_key_t key) = NULL;
int (*pthread_setspecific_fnptr)(pthread_key_t key,
	const void *value) = NULL;
int (*pthread_once_fnptr)(pthread_once_t *, void (*)(void)) = NULL;
pthread_t (*pthread_self_fnptr)(void) = NULL;
int (*pthread_mutex_lock_fnptr)(pthread_mutex_t *mutex) = NULL;
int (*pthread_mutex_unlock_fnptr)(pthread_mutex_t *mutex) = NULL;

/* lua_sb_test_net_addr_match() is in network_sb2if.c */
extern int lua_sb_test_net_addr_match(lua_State *l);

void check_pthread_library(void)
{
	if (pthread_detection_done == 0) {
		/* these are available only in libpthread: */
		pthread_key_create_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_key_create");
		pthread_getspecific_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_getspecific");
		pthread_setspecific_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_setspecific");
		pthread_once_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_once");

		pthread_mutex_lock_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_mutex_lock");
		pthread_mutex_unlock_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_mutex_unlock");

		/* Linux/glibc: pthread_self seems to exist in both
		 * glibc and libpthread. */
		pthread_self_fnptr = dlsym(RTLD_DEFAULT,
			"pthread_self");

		if (pthread_key_create_fnptr &&
		    pthread_getspecific_fnptr &&
		    pthread_setspecific_fnptr &&
		    pthread_once_fnptr) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"pthread library FOUND");
			pthread_detection_done = 1;
			pthread_library_is_available = 1;
		} else if (!pthread_key_create_fnptr &&
		   !pthread_getspecific_fnptr &&
		   !pthread_setspecific_fnptr &&
		   !pthread_once_fnptr) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"pthread library not found");
			pthread_detection_done = -1;
			pthread_library_is_available = 0;
		} else {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"pthread library is only partially available"
				" - operation may become unstable");
			pthread_detection_done = -2;
			pthread_library_is_available = 0;
		}

	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"pthread detection already done (%d)",
			pthread_detection_done);
	}
}

/* ------------ End Of pthreads Warnings & Interface Code ------------ */

