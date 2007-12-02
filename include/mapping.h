/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef MAPPING_H
#define MAPPING_H

#include <sys/types.h>


#define enable_mapping(a) a->mapping_disabled--
#define disable_mapping(a) a->mapping_disabled++

enum lua_engine_states {
	LES_NOT_INITIALIZED = 0,
	LES_INIT_IN_PROCESS,
	LES_READY
};

char *scratchbox_path(const char *func_name, const char *path);
char *scratchbox_path2(const char *binary_name, const char *func_name,
			const char *path);
int sb_argvenvp(const char *binary_name, char ***argv, char ***envp);
char *emumode_map(const char *path);

#endif
