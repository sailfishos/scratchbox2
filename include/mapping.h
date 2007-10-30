/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef MAPPING_H
#define MAPPING_H

#include <sys/types.h>

char *scratchbox_path(const char *func_name, const char *path);
char *scratchbox_path2(const char *binary_name, const char *func_name,
			const char *path);

char *emumode_map(const char *path);

#endif
