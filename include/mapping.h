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

extern char *scratchbox_path(const char *func_name, const char *path,
	int *ro_flagp);
extern char *scratchbox_path3(const char *binary_name, const char *func_name,
		const char *path, const char *mapping_mode, int *ro_flagp);
extern int sb_execve_mod(char **file, char ***argv, char ***envp);
extern char *emumode_map(const char *path);

#endif
