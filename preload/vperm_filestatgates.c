/*
 * libsb2 -- GATE functions for file status simulation (stat(),chown() etc)
 *
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
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

#include <config.h>

#include "sb2.h"
#include "sb2_stat.h"
#include "sb2_vperm.h"
#include "libsb2.h"
#include "exported.h"
#include "rule_tree.h"
#include "rule_tree_rpc.h"

static int get_stat_for_fxxat64(
	const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_filename,
	int flags,
	struct stat64 *statbuf)
{
	int	r;

	r = real_fstatat64(dirfd, mapped_filename->mres_result_path, statbuf, flags);
	if (r < 0) {
		int e = errno;
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d, errno=%d",
			realfnname, r, e);
		errno = e;
	}
	return(r);
}

static void vperm_clear_all_if_virtualized(
        const char *realfnname,
	struct stat64 *statbuf)
{
	ruletree_inodestat_handle_t	handle;
	inodesimu_t			istat_struct;

	ruletree_init_inodestat_handle(&handle, statbuf->st_dev, statbuf->st_ino);
	if (ruletree_find_inodestat(&handle, &istat_struct) == 0) {
		/* vperms exist for this inode */
		if (istat_struct.inodesimu_active_fields != 0) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: clear dev=%llu ino=%llu", 
				realfnname, (unsigned long long)statbuf->st_dev,
				(unsigned long long)statbuf->st_ino);
			ruletree_rpc__vperm_clear(statbuf->st_dev, statbuf->st_ino);
		}
	}
}

/* ======================= stat() variants ======================= */

int __xstat_gate(int *result_errno_ptr,
	int (*real___xstat_ptr)(int ver, const char *filename, struct stat *buf),
        const char *realfnname,
	int ver,
	const mapping_results_t *mapped_filename,
	struct stat *buf)
{
	int	r;
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: calling sb2_stat(%s)",
		realfnname, mapped_filename->mres_result_path);
	r = sb2_stat_file(mapped_filename->mres_result_path, buf, result_errno_ptr,
		real___xstat_ptr, ver, NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: sb2_stat(%s) returned",
		realfnname, mapped_filename->mres_result_path);
	return(r);
}

int __xstat64_gate(int *result_errno_ptr,
	int (*real___xstat64_ptr)(int ver, const char *filename, struct stat64 *buf),
        const char *realfnname,
	int ver,
	const mapping_results_t *mapped_filename,
	struct stat64 *buf)
{
	int	r;
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: calling sb2_stat(%s)",
		realfnname, mapped_filename->mres_result_path);
	r = sb2_stat64_file(mapped_filename->mres_result_path, buf, result_errno_ptr,
		real___xstat64_ptr, ver, NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: sb2_stat(%s) returned",
		realfnname, mapped_filename->mres_result_path);
	return(r);
}

/* int __lxstat(int ver, const char *filename, struct stat *buf) */
int __lxstat_gate(int *result_errno_ptr,
	int (*real___lxstat_ptr)(int ver, const char *filename, struct stat *buf),
        const char *realfnname,
	int ver,
	const mapping_results_t *mapped_filename,
	struct stat *buf)
{
	int	r;
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: calling sb2_stat(%s)",
		realfnname, mapped_filename->mres_result_path);
	r = sb2_stat_file(mapped_filename->mres_result_path, buf, result_errno_ptr,
		real___lxstat_ptr, ver, NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: sb2_stat(%s) returned",
		realfnname, mapped_filename->mres_result_path);
	return(r);
}

/* int __lxstat64(int ver, const char *filename, struct stat64 *buf) */
int __lxstat64_gate(int *result_errno_ptr,
	int (*real___lxstat64_ptr)(int ver, const char *filename, struct stat64 *buf),
        const char *realfnname,
	int ver,
	const mapping_results_t *mapped_filename,
	struct stat64 *buf)
{
	int	r;
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: calling sb2_stat(%s)",
		realfnname, mapped_filename->mres_result_path);
	r = sb2_stat64_file(mapped_filename->mres_result_path, buf, result_errno_ptr,
		real___lxstat64_ptr, ver, NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s gate: sb2_stat(%s) returned",
		realfnname, mapped_filename->mres_result_path);
	return(r);
}

int fstat_gate(int *result_errno_ptr,
	int (*real_fstat_ptr)(int fd, struct stat *buf),
        const char *realfnname,
	int fd,
	struct stat *buf)
{
	int res;

	res = (*real_fstat_ptr)(fd, buf);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, buf, NULL);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int fstat64_gate(int *result_errno_ptr,
	int (*real_fstat64_ptr)(int fd, struct stat64 *buf),
        const char *realfnname,
	int fd,
	struct stat64 *buf)
{
	int res;

	res = (*real_fstat64_ptr)(fd, buf);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, NULL, buf);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int __fxstat_gate(int *result_errno_ptr,
	int (*real___fxstat_ptr)(int ver, int fd, struct stat *buf),
        const char *realfnname,
	int ver,
	int fd,
	struct stat *buf)
{
	int res;

	res = (*real___fxstat_ptr)(ver, fd, buf);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, buf, NULL);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int __fxstat64_gate(int *result_errno_ptr,
	int (*real___fxstat64_ptr)(int ver, int fd, struct stat64 *buf),
        const char *realfnname,
	int ver,
	int fd,
	struct stat64 *buf64)
{
	int res;

	res = (*real___fxstat64_ptr)(ver, fd, buf64);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, NULL, buf64);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int __fxstatat_gate(int *result_errno_ptr,
	int (*real___fxstatat_ptr)(int ver, int dirfd, const char *pathname, struct stat *buf, int flags),
	const char *realfnname,
	int ver,
	int dirfd,
	const mapping_results_t *mapped_filename,
	struct stat *buf,
	int flags)
{
	int	res;

	res = (*real___fxstatat_ptr)(ver, dirfd, mapped_filename->mres_result_path, buf, flags);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, buf, NULL);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int __fxstatat64_gate(int *result_errno_ptr,
	int (*real___fxstatat64_ptr)(int ver, int dirfd, const char *pathname, struct stat64 *buf64, int flags),
	const char *realfnname,
	int ver,
	int dirfd,
	const mapping_results_t *mapped_filename,
	struct stat64 *buf64,
	int flags)
{
	int	res;

	res = (*real___fxstatat64_ptr)(ver, dirfd, mapped_filename->mres_result_path, buf64, flags);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, NULL, buf64);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int stat64_gate(int *result_errno_ptr,
	int (*real_stat64_ptr)(const char *file_name, struct stat64 *buf64),
        const char *realfnname,
	const mapping_results_t *mapped_filename,
	struct stat64 *buf64)
{
	int res;

	res = (*real_stat64_ptr)(mapped_filename->mres_result_path, buf64);
	if (res == 0) {
		i_virtualize_struct_stat(realfnname, NULL, buf64);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

#ifdef HAVE_STATX
int statx_gate(int *result_errno_ptr,
	int (*real_statx_ptr)(int dirfd, const char *__restrict pathname, int flags,
		unsigned int mask, struct statx *__restrict buf),
	const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_filename,
	int flags,
	unsigned int mask,
	struct statx *__restrict buf)
{
	int res;

	res = (*real_statx_ptr)(dirfd, mapped_filename->mres_result_path, flags, mask, buf);
	if (res == 0) {
		i_virtualize_struct_statx(realfnname, buf);
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}
#endif

/* ======================= chown() variants ======================= */

static void vperm_chown(
        const char *realfnname,
	struct stat64 *statbuf,
	uid_t owner,
	gid_t group)
{
	int set_uid = 0;
	int set_gid = 0;
	int release_uid = 0;
	int release_gid = 0;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: real fn=>EPERM, virtualize "
		" (uid=%d, gid=%d)",
		realfnname, statbuf->st_uid, statbuf->st_gid);
	if (owner != (uid_t)-1) {
		if (statbuf->st_uid != owner) set_uid = 1;
		else release_uid = 1;
	}
	if (group != (gid_t)-1) {
		if (statbuf->st_gid != group) set_gid = 1;
		else release_gid = 1;
	}

	if (set_uid || set_gid) {
		ruletree_rpc__vperm_set_ids((uint64_t)statbuf->st_dev,
			(uint64_t)statbuf->st_ino, set_uid, owner, set_gid, group);
	}
	if (release_uid || release_gid) {
		ruletree_rpc__vperm_release_ids((uint64_t)statbuf->st_dev,
			(uint64_t)statbuf->st_ino, release_uid, release_gid);
	}
	
	/* Assume success. FIXME; check the result. */
}

int fchownat_gate(int *result_errno_ptr,
	int (*real_fchownat_ptr)(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_filename,
	uid_t owner,
	gid_t group,
	int flags)
{
	int	res;
	struct stat64 statbuf;

	/* first try the real function */
	res = (*real_fchownat_ptr)(dirfd, mapped_filename->mres_result_path, owner, group, flags);
	if (res == 0) {
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			/* there are active vperm inodestat nodes */
			if (get_stat_for_fxxat64(realfnname, dirfd, mapped_filename, flags, &statbuf) == 0) {
				/* since the real function succeeds, vperm_chown() will now
				 * release simulated UID and/or GID */
				vperm_chown(realfnname, &statbuf, owner, group);
			}
		}
	} else {
		int	e = errno;

		if (e == EPERM) {
			if (get_stat_for_fxxat64(realfnname, dirfd, mapped_filename, flags, &statbuf) == 0) {
				vperm_chown(realfnname, &statbuf, owner, group);
				res = 0;
			} else {
				/* This should never happen */
				*result_errno_ptr = EPERM;
				res = -1;
			}
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "fchownat: returns %d", res);
	return(res);
}

int chown_gate(int *result_errno_ptr,
	int (*real_chown_ptr)(const char *pathname, uid_t owner, gid_t group),
        const char *realfnname,
	const mapping_results_t *mapped_filename,
	uid_t owner,
	gid_t group)
{
	int	res;
	struct stat64 statbuf;

	/* first try the real function */
	res = (*real_chown_ptr)(mapped_filename->mres_result_path, owner, group);
	if (res == 0) {
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			/* there are active vperm inodestat nodes */
			if (real_stat64(mapped_filename->mres_result_path, &statbuf) == 0) {
				/* since the real function succeeds, vperm_chown() will now
				 * release simulated UID and/or GID */
				vperm_chown(realfnname, &statbuf, owner, group);
			}
		}
	} else {
		int	e = errno;

		if (e == EPERM) {
			res = real_stat64(mapped_filename->mres_result_path, &statbuf);
			if (res == 0) vperm_chown(realfnname, &statbuf, owner, group);
			else {
				*result_errno_ptr = EPERM;
				res = -1;
			}
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "chown: returns %d", res);
	return(res);
}

int lchown_gate(int *result_errno_ptr,
	int (*real_lchown_ptr)(const char *pathname, uid_t owner, gid_t group),
        const char *realfnname,
	const mapping_results_t *mapped_filename,
	uid_t owner,
	gid_t group)
{
	int	res;
	struct stat64 statbuf;

	/* first try the real function */
	res = (*real_lchown_ptr)(mapped_filename->mres_result_path, owner, group);
	if (res == 0) {
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			/* there are active vperm inodestat nodes */
			if (real_lstat64(mapped_filename->mres_result_path, &statbuf) == 0) {
				/* since the real function succeeds, vperm_chown() will now
				 * release simulated UID and/or GID */
				vperm_chown(realfnname, &statbuf, owner, group);
			}
		}
	} else {
		int	e = errno;

		if (e == EPERM) {
			res = real_lstat64(mapped_filename->mres_result_path, &statbuf);
			if (res == 0) vperm_chown(realfnname, &statbuf, owner, group);
			else {
				res = -1;
				*result_errno_ptr = EPERM;
			}
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "lchown: returns %d", res);
	return(res);
}

int fchown_gate(int *result_errno_ptr,
	int (*real_fchown_ptr)(int fd, uid_t owner, gid_t group),
        const char *realfnname,
	int fd,
	uid_t owner,
	gid_t group)
{
	int	res;
	struct stat64 statbuf;

	/* first try the real function */
	res = (*real_fchown_ptr)(fd, owner, group);
	if (res == 0) {
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			/* there are active vperm inodestat nodes */
			if (real_fstat64(fd, &statbuf) == 0) {
				/* since the real function succeeds, vperm_chown() will now
				 * release simulated UID and/or GID */
				vperm_chown(realfnname, &statbuf, owner, group);
			}
		}
	} else {
		int	e = errno;

		if (e == EPERM) {
			res = real_fstat64(fd, &statbuf);
			if (res == 0) vperm_chown(realfnname, &statbuf, owner, group);
			else {
				res = -1;
				*result_errno_ptr = EPERM;
			}
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "fchown: returns %d", res);
	return(res);
}

/* ======================= chmod() variants ======================= */

/* set/release st_mode virtualization */
static void vperm_chmod(
        const char *realfnname,
	struct stat64 *statbuf,
	mode_t virt_mode,
	mode_t suid_sgid_bits)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: virtualize stat "
		" (real mode=0%o, new virtual mode=0%o, suid/sgid=0%o)",
		realfnname, statbuf->st_mode, virt_mode, suid_sgid_bits);

	if (((statbuf->st_mode & ~(S_IFMT | S_ISUID | S_ISGID)) !=
		    (virt_mode & ~(S_IFMT | S_ISUID | S_ISGID))) ||
	    ((statbuf->st_mode & (S_ISUID | S_ISGID)) != suid_sgid_bits))
		ruletree_rpc__vperm_set_mode((uint64_t)statbuf->st_dev,
			(uint64_t)statbuf->st_ino, statbuf->st_mode,
			virt_mode & ~(S_IFMT | S_ISUID | S_ISGID),
			suid_sgid_bits & (S_ISUID | S_ISGID));
	else
		ruletree_rpc__vperm_release_mode((uint64_t)statbuf->st_dev,
			(uint64_t)statbuf->st_ino);
	/* Assume success. FIXME; check the result. */
}

static int vperm_chmod_if_simulated_device(
        const char *realfnname,
	struct stat64 *statbuf,
	mode_t mode,
	mode_t suid_sgid_bits)
{
	ruletree_inodestat_handle_t	handle;
	inodesimu_t			istat_struct;

	ruletree_init_inodestat_handle(&handle, statbuf->st_dev, statbuf->st_ino);
	if (ruletree_find_inodestat(&handle, &istat_struct) == 0) {
		/* vperms exist for this inode */
		if (istat_struct.inodesimu_active_fields & RULETREE_INODESTAT_SIM_DEVNODE) {
			/* A simulated device; never set real mode for this,
			 * real mode is 0000 intentionally.  */
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: set mode of simulated device", __func__);
			vperm_chmod(realfnname, statbuf, mode, suid_sgid_bits);
			return(0);
		}
	}
	return(-1);
}

static int vperm_stat_for_chmod(
	const char *realfnname,
	int fd,
	const mapping_results_t *mapped_filename,
	int flags,
	struct stat64 *buf)
{
	if (mapped_filename) {
		return (get_stat_for_fxxat64(realfnname, fd, mapped_filename, flags, buf));
	}
	return (real_fstat64(fd, buf));
}

static void vperm_chmod_prepare(
	const char *realfnname,
	int	fd,
	const mapping_results_t *mapped_filename,
	int	flags,
	struct stat64 *buf,
	mode_t	mode,
	mode_t	suid_sgid_bits,
	int	*has_stat,
	int	*forced_owner_rights,
	int	*return_zero_now)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %s", __func__, realfnname);

	/* A simulated device => don't change real mode at all.*/
	if (get_vperm_num_active_inodestats() > 0) {
		if (vperm_stat_for_chmod(realfnname, fd, mapped_filename, flags, buf) == 0) {
			*has_stat = 1;
			if (vperm_chmod_if_simulated_device(realfnname, buf, mode,
					suid_sgid_bits) == 0) {
				*return_zero_now = 1;
				return;
			}
			/* else not a simulated device, continue here */
		} /* else the real function sets errno */
	}

	/* If simulated root and root FS permission simulation
	 * is required => ensure that directories have RWX 
	 * mode for the owner.
	 *
	 * Trying to set mode to something which
	 * won't be fully usable by the simulated root user might
	 * cause trouble. So we'll "fix" the mode here; That provides
	 * compatibility with fakeroot, but breaks the semantics
	 * for ordinary users..
	 *
	 * (ordinary files will get special treatment in
	 * open() (and "friends", including fopen()), so there
	 * is no need to do this for other objects than
	 * directories here)
	*/
	if (vperm_uid_or_gid_virtualization_is_active() &&
	    (vperm_geteuid() == 0) &&
	    vperm_simulate_root_fs_permissions()) {
		int	is_dir = 0;

		if (!*has_stat) {
			if (vperm_stat_for_chmod(realfnname, fd, mapped_filename, flags, buf) != 0)
				return;
			*has_stat = 1;
		}
		is_dir = S_ISDIR(buf->st_mode);

		if (is_dir && ((mode & 0700) != 0700)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: simulate_root_fs_permissions for directory,"
				" set forced_owner_rights = 0700",
				__func__);
			*forced_owner_rights = 0700;
		}
	}
}

static int vperm_chmod_done_update_state(
	int *result_errno_ptr,
	const char *realfnname,
	int res,
	int e,
	int fd,
	const mapping_results_t *mapped_filename,
	int flags,
	mode_t mode,
	mode_t suid_sgid_bits,
	int forced_owner_rights)
{
	struct stat64	statbuf;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %s", __func__, realfnname);

	/* need to update vperm state if
	 *  - SUID/SGID is used
	 *  - real function failed due to permissions
	 *  - real function succeeded and there are active vperms.
	 *  - always when root FS permissions simulation is
	 *    active (nonzero forced_owner_rights) 
	 * Don't need to update vperms, if
	 *  - nothing active in vperm tree
	 *  - other errors than EPERM.
	*/
	if (suid_sgid_bits ||
	    forced_owner_rights ||
	    ((res < 0) && ( e == EPERM)) ||
	    ((res == 0) && (get_vperm_num_active_inodestats() > 0))) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: set vperms", __func__);

		if (vperm_stat_for_chmod(realfnname, fd, mapped_filename, flags, &statbuf) == 0) {
			vperm_chmod(realfnname, &statbuf, mode, suid_sgid_bits);
			res = 0;
		} else { /* real fn didn't work, and can't stat */
			*result_errno_ptr = e;
			res = -1;
		}
	}
	return(res);
}


int fchmodat_gate(int *result_errno_ptr,
	int (*real_fchmodat_ptr)(int dirfd, const char *pathname, mode_t mode, int flags),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_filename,
	mode_t mode,
	int flags)
{
	int		res;
	struct stat64	statbuf;
	mode_t		suid_sgid_bits;
	int		e = 0;
	int		forced_owner_rights = 0;
	int		has_stat = 0;
	int		return_zero_now = 0;

	/* separate SUID and SGID bits from mode. Never set
	 * real SUID/SGID bits. */
	suid_sgid_bits = mode & (S_ISUID | S_ISGID);
	mode &= ~(S_ISUID | S_ISGID);

	vperm_chmod_prepare(realfnname, dirfd, mapped_filename,
		flags, &statbuf,
		mode, suid_sgid_bits,
		&has_stat, &forced_owner_rights, &return_zero_now);
	if (return_zero_now) return(0);

	/* try the real function */
	res = (*real_fchmodat_ptr)(dirfd, mapped_filename->mres_result_path,
		mode | forced_owner_rights, flags);
	if (res != 0) {
		e = errno;
	}
	/* finalize. */
	res = vperm_chmod_done_update_state(result_errno_ptr,
		realfnname, res, e,
		dirfd, mapped_filename, flags,
		mode, suid_sgid_bits, forced_owner_rights);

	SB_LOG(SB_LOGLEVEL_DEBUG, "fchmodat: returns %d", res);
	return(res);
}

int fchmod_gate(int *result_errno_ptr,
	int (*real_fchmod_ptr)(int fildes, mode_t mode),
        const char *realfnname,
	int fd,
	mode_t mode)
{
	int		res;
	struct stat64	statbuf;
	mode_t		suid_sgid_bits;
	int		e = 0;
	int		forced_owner_rights = 0;
	int		has_stat = 0;
	int		return_zero_now = 0;

	/* separate SUID and SGID bits from mode. Never set
	 * real SUID/SGID bits. */
	suid_sgid_bits = mode & (S_ISUID | S_ISGID);
	mode &= ~(S_ISUID | S_ISGID);

	vperm_chmod_prepare(realfnname, fd, NULL/*no mapped_filename*/,
		0/*no flags*/, &statbuf,
		mode, suid_sgid_bits,
		&has_stat, &forced_owner_rights, &return_zero_now);
	if (return_zero_now) return(0);

	res = (*real_fchmod_ptr)(fd, mode | forced_owner_rights);
	if (res != 0) {
		e = errno;
	}
	res = vperm_chmod_done_update_state(result_errno_ptr,
		realfnname, res, e,
		fd, NULL/*no mapped_filename*/, 0/*no flags*/,
		mode, suid_sgid_bits, forced_owner_rights);

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d", __func__, res);
	return(res);
}

int chmod_gate(int *result_errno_ptr,
	int (*real_chmod_ptr)(const char *path, mode_t mode),
        const char *realfnname,
	const mapping_results_t *mapped_filename,
	mode_t mode)
{
	int		res;
	struct stat64	statbuf;
	mode_t		suid_sgid_bits;
	int		e = 0;
	int		forced_owner_rights = 0;
	int		has_stat = 0;
	int		return_zero_now = 0;

	/* separate SUID and SGID bits from mode. Never set
	 * real SUID/SGID bits. */
	suid_sgid_bits = mode & (S_ISUID | S_ISGID);
	mode &= ~(S_ISUID | S_ISGID);

	vperm_chmod_prepare(realfnname, AT_FDCWD, mapped_filename,
		0/*flags:follow symlinks*/, &statbuf,
		mode, suid_sgid_bits,
		&has_stat, &forced_owner_rights, &return_zero_now);
	if (return_zero_now) return(0);

	res = (*real_chmod_ptr)(mapped_filename->mres_result_path,
		mode | forced_owner_rights);
	if (res != 0) {
		e = errno;
	}
	res = vperm_chmod_done_update_state(result_errno_ptr,
		realfnname, res, e,
		AT_FDCWD, mapped_filename, 0/*flags:follow symlinks*/,
		mode, suid_sgid_bits, forced_owner_rights);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d", __func__, res);
	return(res);
}

/* ======================= mknod() variants ======================= */

/* create a dummy, simulated
 * device node: First create a file to get an
 * inode, then set it to be simulated.
 * (Races are possible (breveen creat() and 
 * vperm_mknod(), but can't be avoided)
*/
static int vperm_mknod(
	int dirfd,
	int *result_errno_ptr,
	const mapping_results_t *mapped_filename,
	mode_t mode,
	dev_t dev)
{
	struct stat64 statbuf;
	int dummy_dev_fd;
	int res;

	/* we can only simulate device nodes. */
	switch (mode & S_IFMT) {
	case S_IFCHR:
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Creating simulated character device node", __func__);
		break;
	case S_IFBLK:
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Creating simulated block device node", __func__);
		break;
	default:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: not a device node, can't simulate (type=0%o)",
			__func__, (mode & S_IFMT));
		*result_errno_ptr = EPERM;
                return(-1);
	}

	/* Create a file without read or write permissions;
	 * the simulated device should not be used ever, and
	 * assuming that the user is not privileged,
	 * it can't be opened...which is exactly what we want.
	*/
	dummy_dev_fd = openat_nomap_nolog(dirfd,
		mapped_filename->mres_result_path, O_CREAT|O_WRONLY|O_TRUNC, 0000);
	if (dummy_dev_fd < 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: failed to create as a file (%s)",
			 __func__, mapped_filename->mres_result_path);
		*result_errno_ptr = EPERM;
		return(-1);
	} 

	res = real_fstat64(dummy_dev_fd, &statbuf);
	close_nomap_nolog(dummy_dev_fd);
	if (res == 0) {
		ruletree_rpc__vperm_set_dev_node((uint64_t)statbuf.st_dev,
			(uint64_t)statbuf.st_ino, mode, (uint64_t)dev);
	} else {
		*result_errno_ptr = EPERM;
		res = -1;
	}
	return(res);
}

int __xmknod_gate(int *result_errno_ptr,
	int (*real___xmknod_ptr)(int ver, const char *path, mode_t mode, dev_t *dev),
        const char *realfnname,
	int ver,
	const mapping_results_t *mapped_filename,
	mode_t mode,
	dev_t *dev)
{
	int	res;

	/* first try the real function */
	res = (*real___xmknod_ptr)(ver, mapped_filename->mres_result_path, mode, dev);
	if (res == 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: real fn: ok", __func__);
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			struct stat64 statbuf;
			/* there are active vperm inodestat nodes */
			if (real_stat64(mapped_filename->mres_result_path, &statbuf) == 0) {
				/* since the real function succeeds, use the real inode
				 * directly and release all simulated information */
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
			}
		}
	} else {
		int	e = errno;

		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: real fn: %d, errno=%d",
			__func__, res, e);
		if (e == EPERM) {
			res = vperm_mknod(AT_FDCWD, result_errno_ptr,
				mapped_filename, mode, *dev);
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d", __func__, res);
	return(res);
}

int __xmknodat_gate(int *result_errno_ptr,
	int (*real___xmknodat_ptr)(int ver, int dirfd, const char *pathname, mode_t mode, dev_t *dev),
        const char *realfnname,
	int ver,
	int dirfd,
	const mapping_results_t *mapped_filename,
	mode_t mode, dev_t *dev)
{
	int	res;

	/* first try the real function */
	res = (*real___xmknodat_ptr)(ver, dirfd, mapped_filename->mres_result_path, mode, dev);
	if (res == 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: real fn: ok", __func__);
		/* OK, success. */
		/* If this inode has been virtualized, update DB */
		if (get_vperm_num_active_inodestats() > 0) {
			struct stat64 statbuf;
			/* there are active vperm inodestat nodes */
			if (get_stat_for_fxxat64(realfnname, dirfd, mapped_filename, 0, &statbuf) == 0) {
				/* since the real function succeeds, use the real inode
				 * directly and release all simulated information */
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
			}
		}
	} else {
		int	e = errno;

		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: real fn: %d, errno=%d",
			__func__, res, e);
		if (e == EPERM) {
			res = vperm_mknod(dirfd, result_errno_ptr,
				mapped_filename, mode, *dev);
		} else {
			*result_errno_ptr = e;
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d", __func__, res);
	return(res);
}


/* ======================= unlink() variants, remove(), rmdir() ======================= */
int unlink_gate(int *result_errno_ptr,
	int (*real_unlink_ptr)(const char *pathname),
        const char *realfnname,
	const mapping_results_t *mapped_filename)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If this inode has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (real_stat64(mapped_filename->mres_result_path, &statbuf) == 0) {
			has_stat = 1;
		}
	}

	res = (*real_unlink_ptr)(mapped_filename->mres_result_path);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d unlinks the file ? */
			if (statbuf.st_nlink == 1)
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int remove_gate(int *result_errno_ptr,
	int (*real_remove_ptr)(const char *pathname),
        const char *realfnname,
	const mapping_results_t *mapped_filename)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If this inode has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (real_stat64(mapped_filename->mres_result_path, &statbuf) == 0) {
			has_stat = 1;
			/* FIXME. Build handle and check if it is DB, don't use IPC every time */
		}
	}

	res = (*real_remove_ptr)(mapped_filename->mres_result_path);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d removes the file ? */
			if (statbuf.st_nlink == 1)
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int rmdir_gate(int *result_errno_ptr,
	int (*real_rmdir_ptr)(const char *pathname),
        const char *realfnname,
	const mapping_results_t *mapped_filename)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If this inode has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (real_stat64(mapped_filename->mres_result_path, &statbuf) == 0) {
			has_stat = 1;
			/* FIXME. Build handle and check if it is DB, don't use IPC every time */
		}
	}

	res = (*real_rmdir_ptr)(mapped_filename->mres_result_path);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d removes the file ? */
			vperm_clear_all_if_virtualized(realfnname, &statbuf);
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int unlinkat_gate(int *result_errno_ptr,
	int (*real_unlinkat_ptr)(int dirfd, const char *pathname, int flags),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_filename,
	int flags)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If this inode has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (get_stat_for_fxxat64(realfnname, dirfd, mapped_filename, 0, &statbuf) == 0) {
			has_stat = 1;
		}
	}

	res = (*real_unlinkat_ptr)(dirfd, mapped_filename->mres_result_path, flags);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d does the operation ? */
			if (statbuf.st_nlink == 1)
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

/* ======================= mkdir() variants ======================= */

static void vperm_set_owner_and_group(
	int dirfd,
	const char *realfnname,
	const mapping_results_t *mapped_pathname)
{
	struct stat64 statbuf;

	if (real_fstatat64(dirfd, mapped_pathname->mres_result_path, &statbuf, 0) == 0) {
		vperm_chown(realfnname, &statbuf, vperm_geteuid(), vperm_getegid());
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: stat failed", realfnname);
	}
}

static void vperm_mkdir_prepare(
	const char *realfnname,
	mode_t	mode,
	int	*forced_owner_rights)
{
	/* If simulated root and root FS permission simulation
	 * is required => ensure that the directory will
	 * have RWX permissions.
	 *
	 * This is needed for fakeroot compatibility.
	 * Trying to set mode to something which
	 * won't be fully usable by the simulated root user might
	 * cause trouble. So we'll "fix" the mode here; That provides
	 * compatibility with fakeroot, but breaks the semantics
	 * for ordinary users..
	 *
	 * There is a similar trick in chmod (and fchmod, etc)
	*/
	if (vperm_uid_or_gid_virtualization_is_active() &&
	    (vperm_geteuid() == 0) &&
	    vperm_simulate_root_fs_permissions()) {
		if ((mode & 0700) != 0700) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: forcing owner rights to RWX", realfnname);
			*forced_owner_rights = 0700;
		}
	}
}

static int vperm_mkdir_finalize(
	const char *realfnname,
	int	dirfd,
	const mapping_results_t *mapped_pathname,
	mode_t	mode,
	int	forced_owner_rights)
{
	if (forced_owner_rights) {
		struct stat64 statbuf;

		if (get_stat_for_fxxat64(realfnname, dirfd, mapped_pathname, 0/*flags*/, &statbuf) != 0) {
			/* Strange. real fn worked, but here we can't stat */
			return(-1);
		}

		vperm_chmod(realfnname, &statbuf, mode, 0);
	}
	return(0);
}

int mkdir_gate(int *result_errno_ptr,
	int (*real_mkdir_ptr)(const char *pathname, mode_t mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	mode_t mode)
{
	int res;
	int e = 0;
	int forced_owner_rights = 0;

	vperm_mkdir_prepare(realfnname, mode, &forced_owner_rights);	
	res = (*real_mkdir_ptr)(mapped_pathname->mres_result_path,
		mode | forced_owner_rights);

	if (res == 0) {
		/* directory was created */
		if (vperm_uid_or_gid_virtualization_is_active())
			vperm_set_owner_and_group(AT_FDCWD, realfnname, mapped_pathname);
		if (forced_owner_rights)
			vperm_mkdir_finalize(realfnname, AT_FDCWD, mapped_pathname, mode, forced_owner_rights);
	} else {
		e = errno;
		*result_errno_ptr = e;
	}
	return (res);
}

int mkdirat_gate(int *result_errno_ptr,
	int (*real_mkdirat_ptr)(int dirfd, const char *pathname, mode_t mode),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_pathname,
	mode_t mode)
{
	int res;
	int e;
	int forced_owner_rights = 0;

	vperm_mkdir_prepare(realfnname, mode, &forced_owner_rights);	
	res = (*real_mkdirat_ptr)(dirfd, mapped_pathname->mres_result_path, mode);

	if (res == 0) {
		/* directory was created */
		if (vperm_uid_or_gid_virtualization_is_active())
			vperm_set_owner_and_group(dirfd, realfnname, mapped_pathname);
		if (forced_owner_rights)
			vperm_mkdir_finalize(realfnname, dirfd, mapped_pathname, mode, forced_owner_rights);
	} else {
		e = errno;
		*result_errno_ptr = e;
	}
	return (res);
}

/* ======================= open() and variants ======================= */
/* open() etc typically ony need to set simulated owner, if the file
 * was created.
*/

static int vperm_multiopen(
	int log_enabled,
	const char *realfnname,
	int (*open_2_ptr)(const char *pathname, int flags),
	int (*open_2va_ptr)(const char *pathname, int flags, ...),
	int (*openat_3_ptr)(int dirfd, const char *pathname, int flags),
	int (*open_3va_ptr)(int dirfd, const char *pathname, int flags, ...),
	int (*creat_ptr)(const char *pathname, mode_t mode),
	FILE *(*fopen_ptr)(const char *path, const char *mode),
	FILE *(*freopen_ptr)(const char *path, const char *mode, FILE *stream),
	FILE **file_ptr, /* in: stream, out:result if function return FILE */
	const char *file_mode, /* for the FILE* functions */
	int dirfd,
	const char *pathname,
	int flags,
	int modebits)
{
	FILE *f = NULL;

        if (log_enabled) {
                if (open_2_ptr)
                        SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fd=%s(path='%s',flags=0x%X)",
                               __func__, realfnname, pathname, flags);
                if (open_2va_ptr)
                        SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fd=%s(path='%s',flags=0x%X,mode=0%o)",
                               __func__, realfnname, pathname, flags, modebits);

                if (openat_3_ptr)
                        SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fd=%s(dirfd=%d,path='%s',flags=0x%X)",
                               __func__, realfnname, dirfd, pathname, flags);
                if (open_3va_ptr)
                        SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fd=%s(dirfd=%d,path='%s',flags=0x%X,mode=0%o)",
                               __func__, realfnname, dirfd, pathname, flags, modebits);
        }
	if (open_2_ptr)
		return ((*open_2_ptr)(pathname, flags));
	if (open_2va_ptr)
		return ((*open_2va_ptr)(pathname, flags, modebits));

	if (openat_3_ptr)
		return ((*openat_3_ptr)(dirfd, pathname, flags));
	if (open_3va_ptr)
		return ((*open_3va_ptr)(dirfd, pathname, flags, modebits));

	if (creat_ptr) {
		if (log_enabled) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fd=%s(path='%s',mode=0%o)",
				__func__, realfnname, pathname, modebits);
		}
		return ((*creat_ptr)(pathname, modebits));
	}

	if (fopen_ptr) {
		assert(file_ptr);
		if (log_enabled) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fileptr=%s(path='%s',mode='%s')",
				__func__, realfnname, pathname, file_mode);
		}
		f = (*fopen_ptr)(pathname, file_mode);
		*file_ptr = f;
		return (f ? 0 : -1);
	}
	if (freopen_ptr) {
		assert(file_ptr);
		if (log_enabled) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: fileptr=%s(path='%s',mode='%s',fileptr)",
				__func__, realfnname, pathname, file_mode);
		}
		f = (*freopen_ptr)(pathname, file_mode, *file_ptr);
		*file_ptr = f;
		return (f ? 0 : -1);
	}

	SB_LOG(SB_LOGLEVEL_ERROR, "%s: Internal error: 'open' function is missing (%s)",
		__func__, realfnname);
	return(-1);
}

static int vperm_do_open(
	int *result_errno_ptr,
	const char *realfnname,
	/* five alternative prototypes for the function, only one should exist:*/
	int (*open_2va_ptr)(const char *pathname, int flags, ...),
	int (*open_3va_ptr)(int dirfd, const char *pathname, int flags, ...),
	int (*creat_ptr)(const char *pathname, mode_t mode),
	int (*open_2_ptr)(const char *pathname, int flags),
	int (*openat_3_ptr)(int dirfd, const char *pathname, int flags),
	FILE *(*fopen_ptr)(const char *path, const char *mode),
	FILE *(*freopen_ptr)(const char *path, const char *mode, FILE *stream),
	FILE **file_ptr, /* for the FILE* functions */
	const char *file_mode, /* for the FILE* functions */
	int dirfd,
	const mapping_results_t *mapped_pathname,
	int flags,
	int modebits)
{
	int res_fd = -1;
	int open_errno = 0;
	int target_exists_beforehand = 0;
	int uid_or_gid_is_virtual = 0;
	struct stat64 orig_stat;

	/* prepare. */
	uid_or_gid_is_virtual = vperm_uid_or_gid_virtualization_is_active();
	if (uid_or_gid_is_virtual) {
		if (real_fstatat64(dirfd, mapped_pathname->mres_result_path, &orig_stat, 0) == 0) {
			target_exists_beforehand = 1;
		}
	}

	/* try to open it */
	res_fd = vperm_multiopen(1, realfnname,
		open_2_ptr, open_2va_ptr, openat_3_ptr, open_3va_ptr, creat_ptr,
		fopen_ptr, freopen_ptr, file_ptr, file_mode,
		dirfd, mapped_pathname->mres_result_path, flags, modebits);

	if (res_fd < 0) {
		open_errno = errno;
		/* open failed. If running as simulated root,
		 * try tricks.. */
		if (uid_or_gid_is_virtual &&
		    (vperm_geteuid() == 0) &&
            	    vperm_simulate_root_fs_permissions()) {
			/* simulated root user. */
			if (target_exists_beforehand &&
			    S_ISREG(orig_stat.st_mode)) {
				/* file exist, but can not be opened.
				 * try if it was a matter of insufficient
				 * permissions. */
				/* FIXME: do not do this for simulated devices. */
				int accmode;
				int need_w;
				uid_t real_euid;

				SB_LOG(SB_LOGLEVEL_DEBUG, "%s: open failed, simulated 'root', errno=%d (%s)",
					realfnname, open_errno, mapped_pathname->mres_result_path);

				switch (open_errno) {
				case EACCES:
				case EPERM:
					accmode = flags & O_ACCMODE;
					need_w = (accmode != O_RDONLY);
					real_euid = vperm_get_real_euid();
					if (real_euid == orig_stat.st_uid) {
						int tmpmode = orig_stat.st_mode |
							(need_w ? (S_IRUSR | S_IWUSR) : S_IRUSR);
						/* owner matches, temporarily change the mode..
						 * Warning: race conditions are possible here,
						 * but this can't be done atomically. */
						SB_LOG(SB_LOGLEVEL_DEBUG, "%s: trying to temporarily change "
							"the mode to 0%o (orig.mode=0%o)",
							realfnname, tmpmode, orig_stat.st_mode);

						if (fchmodat_nomap_nolog(dirfd,
							mapped_pathname->mres_result_path, tmpmode, 0) == 0) {
							/* NO LOGGING IN THIS BLOCK. TRY TO BE QUICK. */
							/* mode was set to tmpmode.
							 * try again; if it won't open now,
							 * we just can't do it. */
							res_fd = vperm_multiopen(0, realfnname,
								open_2_ptr, open_2va_ptr, openat_3_ptr, open_3va_ptr,
								creat_ptr,  fopen_ptr, freopen_ptr, file_ptr, file_mode,
								dirfd, mapped_pathname->mres_result_path,
								flags, modebits);
							if (res_fd < 0) {
								open_errno = errno;
							}
							/* Hopefully the file is open now.
							 * in any case restore orig. mode */
							fchmodat_nomap_nolog(dirfd,
								mapped_pathname->mres_result_path,
								orig_stat.st_mode, 0);
						}
						if (res_fd < 0) {
							SB_LOG(SB_LOGLEVEL_DEBUG, "%s: failed to open it for 'root'",
								realfnname);
						} else {
							SB_LOG(SB_LOGLEVEL_DEBUG, "%s: file is now open, fd=%d",
								realfnname, res_fd);
						}
					}
					break;
				}
			} else {
				/* the file did not exist, or not a regular file. */
				/* FIXME: Here we should see if the open
				 * failed due to insufficient access
				 * rights to the directory. Not implemented
				 * yet. */
				SB_LOG(SB_LOGLEVEL_DEBUG, "%s: failed to open/create file, simulated 'root', errno=%d (%s)",
					realfnname, open_errno, mapped_pathname->mres_result_path);
			}
		}
	} else {
		if (uid_or_gid_is_virtual &&
		    (target_exists_beforehand==0)) {
			/* file was created */
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: created file, setting simulated UID and GID (%s)",
				realfnname, mapped_pathname->mres_result_path);
			vperm_set_owner_and_group(dirfd, realfnname, mapped_pathname);
		}
	}
	if (res_fd < 0) {
		*result_errno_ptr = errno;
	}
	return (res_fd);
}

int open_gate(int *result_errno_ptr,
	int (*real_open_ptr)(const char *pathname, int flags, ...),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		real_open_ptr, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname, flags, mode));
}

int open64_gate(int *result_errno_ptr,
	int (*real_open64_ptr)(const char *pathname, int flags, ...),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		real_open64_ptr, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname, flags, mode));
}

int __open_gate(int *result_errno_ptr,
	int (*real_open_ptr)(const char *pathname, int flags, ...),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		real_open_ptr, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname, flags, mode));
}

int __open64_gate(int *result_errno_ptr,
	int (*real_open64_ptr)(const char *pathname, int flags, ...),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		real_open64_ptr, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname, flags, mode));
}

int openat_gate(int *result_errno_ptr,
	int (*real_openat_ptr)(int dirfd, const char *pathname, int flags, ...),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, real_openat_ptr, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		dirfd, mapped_pathname, flags, mode));
}

int openat64_gate(int *result_errno_ptr,
	int (*real_openat64_ptr)(int dirfd, const char *pathname, int flags, ...),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_pathname,
	int flags,
	int mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, real_openat64_ptr, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		dirfd, mapped_pathname, flags, mode));
}

int __open_2_gate(int *result_errno_ptr,
	int (*real___open_2_ptr)(const char *pathname, int flags),
	const char *realfnname,
	const mapping_results_t *pathname,
	int flags)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, real___open_2_ptr, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, pathname,
		flags, 0777/*mode*/));
}

int __open64_2_gate(int *result_errno_ptr,
	int (*real___open64_2_ptr)(const char *pathname, int flags),
	const char *realfnname,
	const mapping_results_t *pathname,
	int flags)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, real___open64_2_ptr, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, pathname,
		flags, 0777/*mode*/));
}

int __openat_2_gate(int *result_errno_ptr,
	int (*real___openat_2_ptr)(int dirfd, const char *pathname, int flags),
	const char *realfnname,
	int dirfd,
	const mapping_results_t *pathname,
	int flags)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, real___openat_2_ptr,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		dirfd, pathname,
		flags, 0777/*mode*/));
}

int __openat64_2_gate(int *result_errno_ptr,
	int (*real___openat64_2_ptr)(int dirfd, const char *pathname, int flags),
	const char *realfnname,
	int dirfd,
	const mapping_results_t *pathname,
	int flags)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, real___openat64_2_ptr,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		dirfd, pathname,
		flags, 0777/*mode*/));
}

int creat_gate(int *result_errno_ptr,
	int (*real_creat_ptr)(const char *pathname, mode_t mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	mode_t mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, real_creat_ptr, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		O_CREAT|O_WRONLY|O_TRUNC/*flags*/, mode));
}

int creat64_gate(int *result_errno_ptr,
	int (*real_creat64_ptr)(const char *pathname, mode_t mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	mode_t mode)
{
	return (vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, real_creat64_ptr, NULL, NULL,
		NULL, NULL, NULL, NULL, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		O_CREAT|O_WRONLY|O_TRUNC/*flags*/, mode));
}

FILE *fopen_gate(int *result_errno_ptr,
	FILE *(*real_fopen_ptr)(const char *path, const char *mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	const char *mode)
{
	FILE *fp;
	int w_mode = fopen_mode_w_perm(mode);

	vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, NULL, /* fd functions */
		real_fopen_ptr, NULL, &fp, mode, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		(w_mode ? O_RDWR : O_RDONLY)/*flags*/,
		0777);
	return(fp);
}

FILE *fopen64_gate(int *result_errno_ptr,
	FILE *(*real_fopen64_ptr)(const char *path, const char *mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	const char *mode)
{
	FILE *fp;
	int w_mode = fopen_mode_w_perm(mode);

	vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, NULL, /* fd functions */
		real_fopen64_ptr, NULL, &fp, mode, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		(w_mode ? O_RDWR : O_RDONLY)/*flags*/,
		0777);
	return(fp);
}

FILE *freopen_gate(int *result_errno_ptr,
	FILE *(*real_freopen_ptr)(const char *path, const char *mode, FILE *stream),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	const char *mode,
	FILE *stream)
{
	FILE *fp = stream;
	int w_mode = fopen_mode_w_perm(mode);

	vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, NULL, /* fd functions */
		NULL, real_freopen_ptr, &fp, mode, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		(w_mode ? O_RDWR : O_RDONLY)/*flags*/,
		0777);
	return(fp);
}

FILE *freopen64_gate(int *result_errno_ptr,
	FILE *(*real_freopen64_ptr)(const char *path, const char *mode, FILE *stream),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	const char *mode,
	FILE *stream)
{
	FILE *fp = stream;
	int w_mode = fopen_mode_w_perm(mode);

	vperm_do_open(
		result_errno_ptr, realfnname,
		NULL, NULL, NULL, NULL, NULL, /* fd functions */
		NULL, real_freopen64_ptr, &fp, mode, /* FILE* stuff */
		AT_FDCWD, mapped_pathname,
		(w_mode ? O_RDWR : O_RDONLY)/*flags*/,
		0777);
	return(fp);
}

/* ======================= rename(), renameat() ======================= */
/* rename may remove a file as a side effect */

int rename_gate(int *result_errno_ptr,
	int (*real_rename_ptr)(const char *oldpath, const char *newpath),
        const char *realfnname,
	const mapping_results_t *mapped_oldpath,
	const mapping_results_t *mapped_newpath)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If newpath has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (real_lstat64(mapped_newpath->mres_result_path, &statbuf) == 0) {
			has_stat = 1;
		}
	}

	res = (*real_rename_ptr)(mapped_oldpath->mres_result_path, mapped_newpath->mres_result_path);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d does the operation ? */
			/* if it was a file, it was removed if there were 1 links.
			 * but minumum link count for directories is 2.
			*/
			if ((statbuf.st_nlink == 1) ||
			    (S_ISDIR(statbuf.st_mode) && (statbuf.st_nlink == 2))) {
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
			}
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int renameat_gate(int *result_errno_ptr,
	int (*real_renameat_ptr)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath),
        const char *realfnname,
	int olddirfd,
	const mapping_results_t *mapped_oldpath,
	int newdirfd,
	const mapping_results_t *mapped_newpath)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If newpath has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (get_stat_for_fxxat64(realfnname, newdirfd, mapped_newpath, AT_SYMLINK_NOFOLLOW, &statbuf) == 0) {
			has_stat = 1;
		}
	}

	res = (*real_renameat_ptr)(olddirfd, mapped_oldpath->mres_result_path,
		newdirfd, mapped_newpath->mres_result_path);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d does the operation ? */
			/* if it was a file, it was removed if there were 1 links.
			 * but minumum link count for directories is 2.
			*/
			if ((statbuf.st_nlink == 1) ||
			    (S_ISDIR(statbuf.st_mode) && (statbuf.st_nlink == 2))) {
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
			}
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

int renameat2_gate(int *result_errno_ptr,
	int (*real_renameat2_ptr)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags),
        const char *realfnname,
	int olddirfd,
	const mapping_results_t *mapped_oldpath,
	int newdirfd,
	const mapping_results_t *mapped_newpath,
	unsigned int flags)
{
	int has_stat = 0;
	struct stat64 statbuf;
	int res;

	/* If newpath has been virtualized, be prepared to update DB */
	if (get_vperm_num_active_inodestats() > 0) {
		/* there are active vperm inodestat nodes */
		if (get_stat_for_fxxat64(realfnname, newdirfd, mapped_newpath, AT_SYMLINK_NOFOLLOW, &statbuf) == 0) {
			has_stat = 1;
		}
	}

	res = (*real_renameat2_ptr)(olddirfd, mapped_oldpath->mres_result_path,
		newdirfd, mapped_newpath->mres_result_path, flags);
	if (res == 0) {
		if (has_stat) {
			/* FIXME: races are possible here. Might be better if
			 * sb2d does the operation ? */
			/* if it was a file, it was removed if there were 1 links.
			 * but minumum link count for directories is 2.
			*/
			if ((statbuf.st_nlink == 1) ||
			    (S_ISDIR(statbuf.st_mode) && (statbuf.st_nlink == 2))) {
				vperm_clear_all_if_virtualized(realfnname, &statbuf);
			}
		}
	} else {
		*result_errno_ptr = errno;
	}
	return(res);
}

/* ======================= fts_*() functions dealing with stat structures ======================= */

#ifdef HAVE_FTS_H
FTSENT *fts_read_gate(int *result_errno_ptr,
	FTSENT * (*real_fts_read_ptr)(FTS *ftsp),
        const char *realfnname,
	FTS *ftsp)
{
	FTSENT *res;

	res = (*real_fts_read_ptr)(ftsp);
	if (res && (res->fts_statp)) {
		i_virtualize_struct_stat(realfnname, res->fts_statp, NULL);
	} else if (res==NULL) {
		*result_errno_ptr = errno;
	}
	return(res);
}

FTSENT *fts_children_gate(int *result_errno_ptr,
	FTSENT * (*real_fts_children_ptr)(FTS *ftsp, int options),
        const char *realfnname,
	FTS *ftsp,
	int options)
{
	FTSENT *res;

	res = (*real_fts_children_ptr)(ftsp, options);

	/* FIXME: check the "options" condition from glibc */
	if (res && (options != FTS_NAMEONLY) && (get_vperm_num_active_inodestats() > 0)) {
		FTSENT *fep = res;
		while (fep) {
			if (fep->fts_statp)
				i_virtualize_struct_stat(realfnname, fep->fts_statp, NULL);
			fep = fep->fts_link;
		}
	} else if (res==NULL) {
		*result_errno_ptr = errno;
	}
	return(res);
}
#endif /* HAVE_FTS_H */

/* ======================= *access*() functions ======================= */

/* for simulated privileged user (root), check access(), faccessat()
 * or euidaccess() to a path.
*/
static int vperm_root_access(
	int *result_errno_ptr,
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_pathname,
	int mode,
	int flags)
{
	int r;
	struct stat64 statbuf;

	/* note: If write permission is requested, but mapping results say it is a R/O
	 * target, this functions is not called at all (the GATE function
	 * returns EROFS to the caller) => no need to handle EROFS here.
	*/

	r = get_stat_for_fxxat64(realfnname, dirfd, mapped_pathname, flags, &statbuf);
	if (r) {
		/* stat failed => fail. */
		int e = errno;
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: as 'root', stat failed, errno=%d, returns -1",
			realfnname, e);
		*result_errno_ptr = e;
		return(-1);
	}
	/* file exists. */
	if (mode == F_OK) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: as 'root', F_OK test => ok",
			realfnname);
		return(0);
	}

	if (mode & X_OK) {
		/* X is ok for privileged users only
		 * if any of the X bits are set */
		if (statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: as 'root', X_OK test => ok",
				realfnname);
			return(0);
		}
		/* No X permissions */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: as 'root', X_OK test => EACCES",
			realfnname);
		*result_errno_ptr = EACCES;
		return(-1);
	} 
	/* root can read and write anything */
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: as 'root', default = ok",
		realfnname);
	return(0);
}

int euidaccess_gate(int *result_errno_ptr,
	int (*real_euidaccess_ptr)(const char *pathname, int mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int mode)
{
	int r;

	/* If simulated root and root FS permission simulation
	 * is required => don't call the real function, use stat
	 * and return simulated info.
	*/
	if (vperm_uid_or_gid_virtualization_is_active() &&
	    (vperm_geteuid() == 0) &&
	    vperm_simulate_root_fs_permissions()) {
		return(vperm_root_access(result_errno_ptr, realfnname,
			AT_FDCWD, mapped_pathname, mode, 0/*flags*/));
	}

	/* ordinary uid, use the real function */
	r = (*real_euidaccess_ptr)(mapped_pathname->mres_result_path, mode);
	if (r < 0) *result_errno_ptr = errno;
	return(r);
}

int eaccess_gate(int *result_errno_ptr,
	int (*real_eaccess_ptr)(const char *pathname, int mode),
        const char *realfnname,
	const mapping_results_t *pathname,
	int mode)
{
	return(euidaccess_gate(result_errno_ptr, real_eaccess_ptr,
		realfnname, pathname, mode));
}

int access_gate(int *result_errno_ptr,
	int (*real_access_ptr)(const char *pathname, int mode),
        const char *realfnname,
	const mapping_results_t *mapped_pathname,
	int mode)
{
	int r;

	/* If simulated root and root FS permission simulation
	 * is required => don't call the real function, use stat
	 * and return simulated info.
	*/
	if (vperm_uid_or_gid_virtualization_is_active() &&
	    (vperm_getuid() == 0) &&
	    vperm_simulate_root_fs_permissions()) {
		return(vperm_root_access(result_errno_ptr, realfnname,
			AT_FDCWD, mapped_pathname, mode, 0/*flags*/));
	}

	/* ordinary uid, use the real function */
	r = (*real_access_ptr)(mapped_pathname->mres_result_path, mode);
	if (r < 0) *result_errno_ptr = errno;
	return(r);
}

int faccessat_gate(int *result_errno_ptr,
	int (*real_faccessat_ptr)(int dirfd, const char *pathname, int mode, int flags),
        const char *realfnname,
	int dirfd,
	const mapping_results_t *mapped_pathname,
	int mode,
	int flags)
{
	int r;

	/* If simulated root and root FS permission simulation
	 * is required => don't call the real function, use stat
	 * and return simulated info.
	*/
	if (vperm_uid_or_gid_virtualization_is_active() &&
	    ((flags & AT_EACCESS) ? (vperm_geteuid() == 0) : (vperm_getuid() == 0)) &&
	    vperm_simulate_root_fs_permissions()) {
		return(vperm_root_access(result_errno_ptr, realfnname,
			dirfd, mapped_pathname, mode, (flags & ~AT_EACCESS)));
	}

	/* ordinary uid, use the real function */
	r = (*real_faccessat_ptr)(dirfd, mapped_pathname->mres_result_path, mode, flags);
	if (r < 0) *result_errno_ptr = errno;
	return(r);
}

