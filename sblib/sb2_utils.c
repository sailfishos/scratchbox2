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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mapping.h"
#include "sb2.h"


int sb_path_exists(const char *path)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "sb_path_exists testing '%s'",
		path);
#ifdef AT_FDCWD
	/* this is easy, can use faccessat() */
	if (faccessat_nomap_nolog(AT_FDCWD, path, F_OK, AT_SYMLINK_NOFOLLOW) == 0) {
		/* target exists */
		return(1);
	}
#else
	/* Not so easy. The OS does not have faccessat() [it has
	 * been added to Linux 2.6.16 - older systems don't have it]
	 * so the operation must make the check by two separate calls:
	 * First check if the target is symlink (by readlink()),
	 * next check if it is an ordinary file or directoy.
	 * N.B. the result is that this returns 'true' if a symlink
	 * exists, even if the target of the symlink does not
	 * exist. Plain access() returns 'false' if it is a symlink
	 * which points to a non-existent destination)
	 * N.B.2. we'll use readlink because lstat() can't be used, 
	 * full explanation for this can be found in paths.c.
	*/
	{
		char	link_dest[PATH_MAX+1];
		int link_len;

		link_len = readlink_nomap(path, link_dest, PATH_MAX);
		if (link_len > 0) {
			/* was a symlink */
			return(1);
		} else {
			if (access_nomap_nolog(path, F_OK) == 0)
				return(1);
		}
	}
#endif
	return(0);
}

