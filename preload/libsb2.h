/*
 * libsb2 -- scratchbox2 preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 *
 * Heavily based on the libfakechroot library by
 * Piotr Roszatycki <dexter@debian.org>
 */

/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#ifndef LIBSB2_H_INCLUDED_
#define LIBSB2_H_INCLUDED_

#include "config.h"
#include "config_hardcoded.h"

#define __BSD_VISIBLE

#include <assert.h>

#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
//#include <asm/unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <glob.h>
#include <utime.h>
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#ifdef HAVE_FTW_H
#include <ftw.h>
#endif
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

//#include <elf.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>

#include <glob.h>

#include <mapping.h>
#include <sb2.h>

#if defined(PATH_MAX)
#define SBOX_MAXPATH PATH_MAX
#elif defined(_POSIX_PATH_MAX)
#define SBOX_MAXPATH _POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define SBOX_MAXPATH MAXPATHLEN
#else
#define SBOX_MAXPATH 2048
#endif

/* XXX: current code allows ./usr/bin/../../../../../ style escaping
 *      from chroot
 *      if ( amount /../ > amount /other/ ) ) remove extra /../
 */

#define SBOX_MAP_PROLOGUE() \
	char *sbox_path = NULL;

#define SBOX_MAP_AT_PROLOGUE() \
	char *sbox_path = NULL;

#define SBOX_MAP_PATH_NARROW(path, sbox_path, readonly_flag_addr) \
{ \
	if ((path) != NULL && *((char *)(path)) != '\0') { \
		sbox_path = scratchbox_path(__FUNCTION__, path, readonly_flag_addr); \
	} \
}

#define SBOX_MAP_PATH(path, sbox_path, readonly_flag_addr, no_symlink_resolve) \
{ \
	if ((path) != NULL) { \
		sbox_path = scratchbox_path(__FUNCTION__, path, readonly_flag_addr, no_symlink_resolve); \
	} \
}

#define SBOX_MAP_PATH_AT(dirfd, path, sbox_path, readonly_flag_addr, no_symlink_resolve) \
{ \
	if ((path) != NULL) { \
		if (path[0] == '/') { \
			/* absolute path */ \
			sbox_path = scratchbox_path(__FUNCTION__, path, readonly_flag_addr, no_symlink_resolve); \
		} else { \
			sbox_path = strdup(path); \
		}\
	} \
}

#ifndef __GLIBC__
extern char **environ;
#endif

extern void *sbox_find_next_symbol(int log_enabled, const char *functname);

extern int fopen_mode_w_perm(const char *mode);
extern int freopen_errno(FILE *stream);

extern int do_glob (const char *pattern, int flags,
	int (*errfunc) (const char *, int), glob_t *pglob);
#ifdef HAVE_GLOB64
extern int do_glob64 (const char *pattern, int flags,
	int (*errfunc) (const char *, int), glob64_t *pglob);
#endif

extern int sb_execvep(const char *file, char *const argv[], char *const envp[]);

#endif /* ifndef LIBSB2_H_INCLUDED_ */

