/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <config.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysmacros.h>

#include <sb2.h>
#include <sb2_stat.h>
#include <sb2_vperm.h>
#include <rule_tree.h>

#include "libsb2.h"
#include "exported.h"


int real_lstat(const char *path, struct stat *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __lxstat_nomap_nolog(_STAT_VER, path, statbuf);
#else
	r = lstat_nomap_nolog(path, statbuf);
#endif
	return(r);
}

int real_lstat64(const char *path, struct stat64 *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __lxstat64_nomap_nolog(_STAT_VER, path, statbuf);
#else
	r = lstat64_nomap_nolog(path, statbuf);
#endif
	return(r);
}

int real_stat(const char *path, struct stat *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __xstat_nomap_nolog(_STAT_VER, path, statbuf);
#else
	r = stat_nomap_nolog(path, statbuf);
#endif
	return(r);
}

int real_stat64(const char *path, struct stat64 *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __xstat64_nomap_nolog(_STAT_VER, path, statbuf);
#else
	r = stat64_nomap_nolog(path, statbuf);
#endif
	return(r);
}

int real_fstatat(int dirfd, const char *path, struct stat *statbuf, int flags)
{
	int	r;

#ifdef _STAT_VER
	r = __fxstatat_nomap_nolog(_STAT_VER, dirfd, path, statbuf, flags);
#else
	r = fstatat_nomap_nolog(dirfd, path, statbuf, flags);
#endif
	return(r);
}

int real_fstatat64(int dirfd, const char *path, struct stat64 *statbuf, int flags)
{
	int	r;

#ifdef _STAT_VER
	r = __fxstatat64_nomap_nolog(_STAT_VER, dirfd, path, statbuf, flags);
#else
	r = fstatat64_nomap_nolog(dirfd, path, statbuf, flags);
#endif
	return(r);
}

int real_fstat(int fd, struct stat *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __fxstat_nomap_nolog(_STAT_VER, fd, statbuf);
#else
	r = fstat_nomap_nolog(fd, statbuf);
#endif
	return(r);
}

int real_fstat64(int fd, struct stat64 *statbuf)
{
	int	r;

#ifdef _STAT_VER
	r = __fxstat64_nomap_nolog(_STAT_VER, fd, statbuf);
#else
	r = fstat64_nomap_nolog(fd, statbuf);
#endif
	return(r);
}

/* return 0 if not modified, positive if something was virtualized.
 * only one of {buf,bufx,buf64} should be set; set the other ones to NULL */
static int i_virtualize_struct_stat_internal(
	const char *realfnname
	, struct stat *buf
	, struct stat64 *buf64
#ifdef HAVE_STATX
	, struct statx *bufx
#endif
	)
{
	int				res = 0;
	ruletree_inodestat_handle_t	handle;
	inodesimu_t      		istat_in_db;
	uid_t				uf_uid;
	gid_t				uf_gid;

	ruletree_clear_inodestat_handle(&handle);
	if (buf) {
		ruletree_init_inodestat_handle(&handle, buf->st_dev, buf->st_ino);
#ifdef HAVE_STATX
	} else if (bufx) {
		ruletree_init_inodestat_handle(&handle,
			gnu_dev_makedev(bufx->stx_dev_major, bufx->stx_dev_minor),
			bufx->stx_ino);
#endif
	} else {
		ruletree_init_inodestat_handle(&handle, buf64->st_dev, buf64->st_ino);
	}

	if ((get_vperm_num_active_inodestats() > 0) &&
	    (ruletree_find_inodestat(&handle, &istat_in_db) == 0)) {
		int set_uid_gid_of_unknown = vperm_set_owner_and_group_of_unknown_files(
			&uf_uid, &uf_gid);

		SB_LOG(SB_LOGLEVEL_DEBUG, "%s/%s: inodestats struct found:", realfnname, __func__);
		if (istat_in_db.inodesimu_active_fields &
			RULETREE_INODESTAT_SIM_UID) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: found, set uid to %d",
				realfnname, __func__, istat_in_db.inodesimu_uid);
			if (buf) buf->st_uid = istat_in_db.inodesimu_uid;
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_uid = istat_in_db.inodesimu_uid;
#endif
			else   buf64->st_uid = istat_in_db.inodesimu_uid;
			res++;
		} else if (set_uid_gid_of_unknown) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: 'unknown' file, set owner to %d",
					realfnname, __func__, uf_uid);
			if (buf) buf->st_uid = uf_uid;
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_uid = uf_uid;
#endif
			else   buf64->st_uid = uf_uid;
			res++;
		}

		if (istat_in_db.inodesimu_active_fields &
			RULETREE_INODESTAT_SIM_GID) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: found, set gid to %d",
				realfnname, __func__, istat_in_db.inodesimu_gid);
			if (buf) buf->st_gid = istat_in_db.inodesimu_gid;
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_gid = istat_in_db.inodesimu_gid;
#endif
			else   buf64->st_gid = istat_in_db.inodesimu_gid;
			res++;
		} else if (set_uid_gid_of_unknown) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: 'unknown' file, set group to %d",
					realfnname, __func__, uf_gid);
			if (buf) buf->st_gid = uf_gid;
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_gid = uf_gid;
#endif
			else   buf64->st_gid = uf_gid;
			res++;
		}

		if (istat_in_db.inodesimu_active_fields &
			RULETREE_INODESTAT_SIM_MODE) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: found, set mode to 0%o",
				realfnname, __func__, istat_in_db.inodesimu_mode);
			if (buf) buf->st_mode =
					(buf->st_mode & S_IFMT) |
					(istat_in_db.inodesimu_mode & (~S_IFMT));
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_mode =
					(bufx->stx_mode & S_IFMT) |
					(istat_in_db.inodesimu_mode & (~S_IFMT));
#endif
			else   buf64->st_mode =
					(buf64->st_mode & S_IFMT) |
					(istat_in_db.inodesimu_mode & (~S_IFMT));
			res++;
		}
		if (istat_in_db.inodesimu_active_fields &
			RULETREE_INODESTAT_SIM_SUIDSGID) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: found, set suid/sgid 0%o",
				realfnname, __func__, istat_in_db.inodesimu_suidsgid);
			if (buf) buf->st_mode =
				(buf->st_mode & ~(S_ISUID | S_ISGID)) |
				(istat_in_db.inodesimu_suidsgid & (S_ISUID | S_ISGID));
#ifdef HAVE_STATX
			else if (bufx) bufx->stx_mode =
				(bufx->stx_mode & ~(S_ISUID | S_ISGID)) |
				(istat_in_db.inodesimu_suidsgid & (S_ISUID | S_ISGID));
#endif
			else   buf64->st_mode =
				(buf64->st_mode & ~(S_ISUID | S_ISGID)) |
				(istat_in_db.inodesimu_suidsgid & (S_ISUID | S_ISGID));
			res++;
		}
		if (istat_in_db.inodesimu_active_fields &
			RULETREE_INODESTAT_SIM_DEVNODE) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s/%s: found, simulated device 0%o,0x%X",
				realfnname, __func__, istat_in_db.inodesimu_devmode,
				(int)istat_in_db.inodesimu_rdev);
			if (buf) {
				buf->st_mode =
					(buf->st_mode & (~S_IFMT)) |
					(istat_in_db.inodesimu_devmode & S_IFMT);
				buf->st_rdev = istat_in_db.inodesimu_rdev;
#ifdef HAVE_STATX
			} else if (bufx) {
				bufx->stx_mode =
					(bufx->stx_mode & (~S_IFMT)) |
					(istat_in_db.inodesimu_devmode & S_IFMT);
				bufx->stx_rdev_major = major(istat_in_db.inodesimu_rdev);
				bufx->stx_rdev_minor = minor(istat_in_db.inodesimu_rdev);
#endif
			} else {
				buf64->st_mode =
					(buf64->st_mode & (~S_IFMT)) |
					(istat_in_db.inodesimu_devmode & S_IFMT);
				buf64->st_rdev = istat_in_db.inodesimu_rdev;
			}
			res++;
		}
	} else if (vperm_set_owner_and_group_of_unknown_files(&uf_uid, &uf_gid)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s/%s: 'unknown' file, set owner and group to %d.%d",
				realfnname, __func__, uf_uid, uf_gid);
		if (buf) buf->st_uid = uf_uid;
#ifdef HAVE_STATX
		else if (bufx) bufx->stx_uid = uf_uid;
#endif
		else   buf64->st_uid = uf_uid;
		if (buf) buf->st_gid = uf_gid;
#ifdef HAVE_STATX
		else if (bufx) bufx->stx_gid = uf_gid;
#endif
		else   buf64->st_gid = uf_gid;
		res += 2;
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s/%s: done, result=%d", realfnname, __func__, res);
	return(res);
}

/* return 0 if not modified, positive if something was virtualized.
 * only one of {buf,buf64} should be set; set the other one to NULL */
int i_virtualize_struct_stat(
	const char *realfnname,
	struct stat *buf,
	struct stat64 *buf64)
{
	return i_virtualize_struct_stat_internal(realfnname
		, buf
		, buf64
#ifdef HAVE_STATX
		, NULL
#endif
		);
}

#ifdef HAVE_STATX
int i_virtualize_struct_statx(
	const char *realfnname,
	struct statx *buf)
{
	return i_virtualize_struct_stat_internal(realfnname
		, NULL
		, NULL
		, buf);
}
#endif

int sb2_stat_file(const char *path, struct stat *buf, int *result_errno_ptr,
	int (*statfn_with_ver_ptr)(int ver, const char *filename, struct stat *buf),
	int ver,
	int (*statfn_ptr)(const char *filename, struct stat *buf))
{
	int		res;

	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat(%s)", path);
	if (statfn_with_ver_ptr) {
		/* glibc; it doesn't have a real stat() function */
		res = (*statfn_with_ver_ptr)(ver, path, buf);
	} else if (statfn_ptr) {
		/* probably something else than glibc.
		 * hope this works (not tested...) */
		res = (*statfn_ptr)(path, buf);
	} else {
		*result_errno_ptr = EINVAL;
		return(-1);
	}

	if (res == 0) {
		i_virtualize_struct_stat(__func__, buf, NULL);
	} else {
		*result_errno_ptr = errno;
		SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat(%s) : failed, errno=%d",
			path, errno);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat(%s) returns %d", path, res);
	return(res);
}

int sb2_stat64_file(const char *path, struct stat64 *buf, int *result_errno_ptr,
	int (*stat64fn_with_ver_ptr)(int ver, const char *filename, struct stat64 *buf),
	int ver,
	int (*stat64fn_ptr)(const char *filename, struct stat64 *buf))
{
	int		res;

	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat64(%s)", path);
	if (stat64fn_with_ver_ptr) {
		/* glibc; it doesn't have a real stat64() function */
		res = (*stat64fn_with_ver_ptr)(ver, path, buf);
	} else if (stat64fn_ptr) {
		/* probably something else than glibc.
		 * hope this works (not tested...) */
		res = (*stat64fn_ptr)(path, buf);
	} else {
		*result_errno_ptr = EINVAL;
		return(-1);
	}

	if (res == 0) {
		i_virtualize_struct_stat(__func__, NULL, buf);
	} else {
		*result_errno_ptr = errno;
		SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat64(%s) : failed, errno=%d",
			path, errno);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_stat64(%s) returns %d", path, res);
	return(res);
}

int sb2_fstat64(int fd, struct stat64 *statbuf)
{
	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_fstat(%d)", fd);
	if (real_fstat64(fd, statbuf) < 0) return(-1);

	i_virtualize_struct_stat(__func__, NULL, statbuf);
	return(0);
}

