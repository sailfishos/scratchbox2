/*
 * Copyright (C) 2006 Lauri Leukkunen <lleukkun@cc.hut.fi>
 *
 * Licensed under LGPL version 2, see top level LICENSE file for details.
 */

#ifndef LUA_BINDINGS_H
#define LUA_BINDINGS_H

char *scratchbox_path(const char *func_name, const char *path);
#include <sys/types.h>

typedef pid_t pidfunction(void);

void bind_set_getpid(pidfunction *func);

#endif
