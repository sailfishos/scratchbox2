/*
 * fdpathdb.c -- File descriptor => pathname storage
 *		 (correct implementation of openat(), faccessat(), etc. requires
 *		 that pathnames can be easily found for open file descriptors)
 *
 * Copyright (C) 2008 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "libsb2.h"
#include "exported.h"

typedef struct fd_path_db_entry_s {
	char	*fpdb_path;
} fd_path_db_entry_t;

static fd_path_db_entry_t *fd_path_db = NULL;
static int fd_path_db_slots = 0;

static pthread_mutex_t	fd_path_db_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Functions to lock/unlock the mutex, if libpthreads is available.
 * If it isn't, this is used in a sigle-threaded program and we can
 * safely live without the mutex.
*/
static void fdpathdb_mutex_lock(void)
{
	if (pthread_library_is_available) {
		SB_LOG(SB_LOGLEVEL_NOISE2, "Going to lock fd_path_db_mutex");
		(*pthread_mutex_lock_fnptr)(&fd_path_db_mutex);
		/* NO logging here! */
	}
}
static void fdpathdb_mutex_unlock(void)
{
	if (pthread_library_is_available) {
		/* NO logging here! */
		(*pthread_mutex_unlock_fnptr)(&fd_path_db_mutex);
		SB_LOG(SB_LOGLEVEL_NOISE2, "unlocked fd_path_db_mutex");
	}
}

const char *fdpathdb_find_path(int fd)
{
	const char *ret = NULL;

	fdpathdb_mutex_lock();
	{
		/* NOTE: This is a critical section:
		 * - Do not return from this block, mutex is locked !!
		 * - Do not call the logger from this block !!
		*/
		if ((fd >= 0) &&
		    (fd_path_db_slots > fd) &&
		    fd_path_db[fd].fpdb_path) {
			ret = fd_path_db[fd].fpdb_path;
		}
	}
	fdpathdb_mutex_unlock();

	if (ret) {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"fdpathdb_find_path: FD %d => '%s'",
			fd, fd_path_db[fd].fpdb_path);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"fdpathdb_find_path: No pathname for FD %d", fd);
	}

	return(ret);
}

static void fdpathdb_register_mapped_path(
	const char *realfnname, int fd,
	const char *mapped_path, const char *orig_path)
{
	const char *path = NULL;

	if (fd < 0) return;

	if (orig_path && mapped_path) {
		if (*orig_path == '/') {
			/* orig.path is an absolute path, use that directly */
			path = orig_path;
		} else {
			/* Oops. Should not come here; "orig_path"
			 * should be absolute. But if it isn't, 
			 * try to use mapped_path. */
			if (*mapped_path == '/') {
				path = mapped_path;
			} else {
				SB_LOG(SB_LOGLEVEL_ERROR,
					"Internal error: fdpathdb needs absolute"
					" paths (but got '%s','%s')",
					mapped_path, orig_path);
				path = NULL; /* clear the entry */
			}
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE, "%s: Register %d => '%s'",
		realfnname, fd, path ? path : "(NULL path)");

	fdpathdb_mutex_lock();
	{
		/* NOTE: This is a critical section:
		 * - Do not return from this block, mutex is locked !!
		 * - Do not call the logger from this block !!
		*/

		if (fd_path_db_slots <= fd) {
			if (fd_path_db) {
				int slots_after_realloc = fd + 1;
				int new_slots = slots_after_realloc -
					fd_path_db_slots;

				/* allocate more slots */
				fd_path_db = realloc(fd_path_db,
					slots_after_realloc *
						sizeof(fd_path_db_entry_t));
				/* realloc() does not initialize memory;
				 * clear new slots */
				memset(fd_path_db + fd_path_db_slots, 0,
					new_slots * sizeof(fd_path_db_entry_t));

				fd_path_db_slots = slots_after_realloc;
			} else {
				/* allocate a new table */
				fd_path_db_slots = fd + 1;
				fd_path_db = calloc(fd_path_db_slots,
					sizeof(fd_path_db_entry_t));
			}
		}

		if (fd_path_db[fd].fpdb_path) {
			/* overwriting an old entry */
			free(fd_path_db[fd].fpdb_path);
			fd_path_db[fd].fpdb_path = NULL;
		}

		fd_path_db[fd].fpdb_path = path ? strdup(path) : NULL;
	}
	fdpathdb_mutex_unlock();
}

static void fdpathdb_register_mapping_result(const char *realfnname,
	int ret_fd, mapping_results_t *res, const char *pathname)
{
	/* "mres_result_buf" is supposed to be an absolute path,
	 * while "mres_result_path" may be relative or absolute.
	 * (the path DB can not use relative paths)
	*/
	if (*pathname == '/') {
		/* also the original virtual path is absolute */
		fdpathdb_register_mapped_path(realfnname, ret_fd,
			res->mres_result_buf, pathname);
	} else {
		/* orig. path is relative. */
		char	*abs_virtual_path = NULL;

		if (res->mres_virtual_cwd) {
			/* good, virtual CWD is available */
			if(asprintf(&abs_virtual_path, "%s/%s",
			    res->mres_virtual_cwd, pathname) < 0) {
				/* asprintf failed */
				abort();
			}
			SB_LOG(SB_LOGLEVEL_NOISE,
				"fdpathdb_register_mapping_result:"
				" built abs.path '%s'",
				abs_virtual_path);
			fdpathdb_register_mapped_path(realfnname, ret_fd,
				res->mres_result_buf, abs_virtual_path);
			free(abs_virtual_path);
		} else {
			/* virtual path is relative, and it can't
			 * be converted to absolute. 
			 * fdpathdb_register_mapped_path() will try to
			 * use the mapped path instead.
			*/
			fdpathdb_register_mapped_path(realfnname, ret_fd,
				res->mres_result_buf, pathname);
		}
	}
}

/* Wrappers' postprocessors: these register paths to this DB */

extern void __open_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags, int mode)
{
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void __open_2_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags)
{
	(void)flags;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void __open64_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags, int mode)
{
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void __open64_2_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags)
{
	(void)flags;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void open_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags, int mode)
{
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void open64_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	const char *pathname, int flags, int mode)
{
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void openat_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	int dirfd, const char *pathname, int flags, int mode)
{
	(void)dirfd;
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void __openat_2_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	int dirfd, const char *pathname, int flags)
{
	(void)dirfd;
	(void)flags;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void __openat64_2_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	int dirfd, const char *pathname, int flags)
{
	(void)dirfd;
	(void)flags;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

extern void openat64_postprocess_pathname(
	const char *realfnname, int ret_fd, mapping_results_t *res,
	int dirfd, const char *pathname, int flags, int mode)
{
	(void)dirfd;
	(void)flags;
	(void)mode;
	fdpathdb_register_mapping_result(realfnname, ret_fd, res, pathname);
}

void dup_postprocess_(const char *realfnname, int ret, int fd)
{
	const char	*cp = NULL;

	if (ret >= 0) {
		cp = fdpathdb_find_path(fd);
		if (cp) cp = strdup(cp);
		fdpathdb_register_mapped_path(realfnname, ret, cp, cp);
	}
}

void dup2_postprocess_(const char *realfnname, int ret, int fd, int fd2)
{
	const char	*cp = NULL;

	if ((ret >= 0) && (fd != fd2)) {
		cp = fdpathdb_find_path(fd);
		if (cp) cp = strdup(cp);
		fdpathdb_register_mapped_path(realfnname, fd2, cp, cp);
	}
}

void dup3_postprocess_(const char *realfnname, int ret, int fd, int fd2, int flags)
{
	const char	*cp = NULL;

	(void)flags;
	if ((ret >= 0) && (fd != fd2)) {
		cp = fdpathdb_find_path(fd);
		if (cp) cp = strdup(cp);
		fdpathdb_register_mapped_path(realfnname, fd2, cp, cp);
	}
}

void close_postprocess_(const char *realfnname, int ret, int fd)
{
	(void)ret;
	fdpathdb_register_mapped_path(realfnname, fd, NULL, NULL);
}

void fcntl_postprocess_(const char *realfnname, int ret,
	int fd, int cmd, void *arg)
{
	const char	*cp = NULL;

	(void)cp;
	(void)arg;

	switch (cmd) {
	case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
	case F_DUPFD_CLOEXEC:
#endif
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: cmd==dup %d -> %d",
			realfnname, fd, ret);
		dup_postprocess_(realfnname, ret, fd);
		break;
	}
}

void fcntl64_postprocess_(const char *realfnname, int ret,
	int fd, int cmd, void *arg)
{
	fcntl_postprocess_(realfnname, ret, fd, cmd, arg);
}


