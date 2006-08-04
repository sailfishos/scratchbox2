/*
 * Copyright (C) 2005 Nokia
 * Author: Toni Timonen <ttimonen@movial.fi>
 *
 * This file may be used, modified, and distributed under the terms
 * of the GNU General Public License version 2.
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#include "sb_env.h"


#define BUFFERSIZE PATH_MAX

#ifdef DEBUG
#define DEBUG_OUT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_OUT(...)
#endif


//extern char **environ;


#define ENV_RENAME_PREFIX "SBOX_ENV_"
#define ENV_RENAME_UNSET  "(UNSET)"

static void modify_env_vector(char const* *src, char const* entry, char const* newentry)
{
	const char *key;
	char const **dest, *value;
	size_t len = 0;

	key = newentry;

	/* Length of the key (including '='). */
	value = strchr(key, '=');
	if (value) {
		value++;
		len = value - key;
	}

	/* Copy the vector onto itself skipping ENTRY and the key that
	 * equals KEY. */
	for (dest = src; *src; src++) {
		if (*src != entry && strncmp(*src, key, len) != 0) {
			*dest++ = *src;
		}
	}

	/* Put NEWENTRY at the end of the vector if
	 * its value is not "(UNSET)". */
	if (value && strcmp(value, ENV_RENAME_UNSET) != 0) {
		*dest++ = newentry;
	}

	/* Nul-terminate the vector (which has a nul-terminator at its
	 * old end). */
	while (*dest) {
		*dest++ = NULL;
	}
}

static size_t get_envsize(char const*const*an_env) {
	size_t i;
	for (i = 0; an_env[i]; ++i)
		;

	return (i+1)*sizeof(char*);
}

char const* * override_sbox_env(char const* const*oldenv)
{
	size_t envsize;
	char const* *newenv;
	char const* const*p,*entry;
	const size_t prelen = strlen(ENV_RENAME_PREFIX);


	envsize = get_envsize(oldenv);

	newenv = malloc(envsize);
	if (!newenv) {
		fprintf(stderr, "libsb: %s\n", strerror(errno));
		return NULL;
		//oldenv;
	}

	memcpy(newenv, oldenv, envsize);


	/* Strip ENV_RENAME_PREFIX. */
	/* Find SBOX_ENV-entries. */
	for (p=oldenv; *p; p++) {
		entry = *p;
		if (strncmp(entry, ENV_RENAME_PREFIX, prelen) == 0) {
			modify_env_vector(newenv, entry, entry + prelen);
		}
	}

	return newenv;
}

void replace_sbox_env(char const* *oldenv) {
	char const* *newenv;
	newenv=override_sbox_env(oldenv);
	memcpy(oldenv,newenv,get_envsize(newenv));
	free(newenv);
}
