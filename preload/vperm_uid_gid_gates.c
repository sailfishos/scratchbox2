/* 
 * libsb2 -- GATE functions for UID/GID simulation (setuid(),getuid() etc)
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

#include "sb2.h"
#include "libsb2.h"
#include "exported.h"
#include "sb2_vperm.h"


static struct vperm_uids_gids_s {
	int	initialized;

	uid_t	v_uid;
	uid_t	v_euid;
	uid_t	v_saved_uid;
	uid_t	v_fsuid; /* Linux specific */

	gid_t	v_gid;
	gid_t	v_egid;
	gid_t	v_saved_gid;
	gid_t	v_fsgid; /* Linux specific */

	int	v_set_owner_and_group_of_unknown_files;
	uid_t	v_unknown_file_owner;
	gid_t	v_unknown_file_group;

	int	v_simulate_root_fs_permissions;
} vperm_simulated_ids = {0, 0,0,0,0, 0,0,0,0, 0,0,0, 1};

static uid_t	v_real_euid = 0;
static uid_t	v_real_egid = 0;

static void initialize_simulated_ids(void)
{
	const char	*cp;
	int		i1,i2,i3,i4;

	if (vperm_simulated_ids.initialized) return;

	v_real_euid = geteuid_nomap_nolog();
	v_real_egid = getegid_nomap_nolog();
	
	/* UIDs */
	if (sbox_vperm_ids && (cp = strchr(sbox_vperm_ids, 'u')) &&
	    (sscanf(cp, "u%d:%d:%d:%d", &i1, &i2, &i3, &i4) == 4)) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Initializing UIDs from env: %d %d %d %d",
			__func__, i1, i2, i3, i4);
		vperm_simulated_ids.v_uid = i1;
		vperm_simulated_ids.v_euid = i2;
		vperm_simulated_ids.v_saved_uid = i3;
		vperm_simulated_ids.v_fsuid = i4;
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Initializing UIDs from OS.",
			__func__);
		vperm_simulated_ids.v_euid = v_real_euid;
		vperm_simulated_ids.v_uid = getuid_nomap_nolog();
		vperm_simulated_ids.v_fsuid = getuid_nomap_nolog(); /* FIXME */
		vperm_simulated_ids.v_saved_uid = vperm_simulated_ids.v_euid;
	}

	/* GIDs */
	if (sbox_vperm_ids && (cp = strchr(sbox_vperm_ids, 'g')) &&
	    (sscanf(cp, "g%d:%d:%d:%d", &i1, &i2, &i3, &i4) == 4)) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Initializing GIDs from env: %d %d %d %d",
			__func__, i1, i2, i3, i4);
		vperm_simulated_ids.v_gid = i1;
		vperm_simulated_ids.v_egid = i2;
		vperm_simulated_ids.v_saved_gid = i3;
		vperm_simulated_ids.v_fsgid = i4;
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Initializing GIDs from OS.",
			__func__);
		vperm_simulated_ids.v_egid = v_real_egid;
		vperm_simulated_ids.v_gid = getgid_nomap_nolog();
		vperm_simulated_ids.v_fsgid = getgid_nomap_nolog(); /* FIXME */
		vperm_simulated_ids.v_saved_gid = vperm_simulated_ids.v_egid;
	}

	if (sbox_vperm_ids && (cp = strchr(sbox_vperm_ids, 'f')) &&
	    (sscanf(cp, "f%d.%d", &i1, &i2) == 2)) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: Initializing unknown file owner and group info from env: %d %d",
			__func__, i1, i2);
		vperm_simulated_ids.v_set_owner_and_group_of_unknown_files = 1;
		vperm_simulated_ids.v_unknown_file_owner = i1;
		vperm_simulated_ids.v_unknown_file_group = i2;
	}

	if (sbox_vperm_ids && (cp = strchr(sbox_vperm_ids, 'p'))) {
		vperm_simulated_ids.v_simulate_root_fs_permissions = 0;
	}

	vperm_simulated_ids.initialized = 1;
}

uid_t vperm_geteuid(void)
{
	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();
	return(vperm_simulated_ids.v_euid);
}

uid_t vperm_get_real_euid(void)
{
	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();
	return(v_real_euid);
}

gid_t vperm_getegid(void)
{
	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();
	return(vperm_simulated_ids.v_egid);
}

int vperm_uid_or_gid_virtualization_is_active(void)
{
	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();
	return((v_real_euid != vperm_simulated_ids.v_euid) ||
	       (v_real_egid != vperm_simulated_ids.v_egid));
}

int vperm_simulate_root_fs_permissions(void)
{
	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();
	return(vperm_simulated_ids.v_simulate_root_fs_permissions);
}

int vperm_set_owner_and_group_of_unknown_files(uid_t *uidp, gid_t *gidp)
{
	if (uidp) *uidp = vperm_simulated_ids.v_unknown_file_owner;
	if (gidp) *gidp = vperm_simulated_ids.v_unknown_file_group;
	return(vperm_simulated_ids.v_set_owner_and_group_of_unknown_files);
}

char *vperm_export_ids_as_string_for_exec(const char *prefix,
	int mode, uid_t suid, gid_t sgid, const char *user_vperm_request)
{
	char	*r;
	uid_t	exec_euid;
	gid_t	exec_egid;
	uid_t	new_saveduid;
	gid_t	new_savedgid;
	char	ufbuf[100];

	if (vperm_simulated_ids.initialized == 0) {
		if ((mode & (S_ISUID | S_ISGID)) == 0) {
			/* not initialized and not SUID/SGID, reuse existing value */
			if (asprintf(&r, "%s%s", prefix, sbox_vperm_ids) < 0)
				return(NULL);
		}
		/* else need to do SUID/SGID simulation. */
		initialize_simulated_ids();
	} 

	if (!mode && user_vperm_request) {
		/* no SUID/SGID bits, but user-requested change
		 * (usually set from our "fakeroot" wrapper) */
		const char *cp = strchr(user_vperm_request, '=');
		cp = cp ? cp + 1 : user_vperm_request;
		if (asprintf(&r, "%s%s", prefix, cp) < 0) return(NULL);
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: user_vperm_request => '%s'",
			__func__, r);
		return(r);
	}

	if (mode & S_ISUID) {
		exec_euid = suid;
	} else {
		exec_euid = vperm_simulated_ids.v_euid;
	}
	if (mode & S_ISGID) {
		exec_egid = sgid;
	} else {
		exec_egid = vperm_simulated_ids.v_egid;
	}
	new_saveduid = exec_euid;
	new_savedgid = exec_egid;

	if (vperm_simulated_ids.v_set_owner_and_group_of_unknown_files) {
		snprintf(ufbuf, sizeof(ufbuf), ",f%d.%d",
			vperm_simulated_ids.v_unknown_file_owner,
			vperm_simulated_ids.v_unknown_file_group);
	} else {
		ufbuf[0] = '\0';
	}

	if (asprintf(&r, "%su%d:%d:%d:%d,g%d:%d:%d:%d%s%s", prefix,
	     (int)vperm_simulated_ids.v_uid,
	     (int)exec_euid,
	     (int)new_saveduid,
	     (int)vperm_simulated_ids.v_fsuid, /* FIXME: Is this ok or wrong? */
	     (int)vperm_simulated_ids.v_gid,
	     (int)exec_egid,
	     (int)new_savedgid,
	     (int)vperm_simulated_ids.v_fsgid, /* FIXME: Is this ok or wrong? */
	     ufbuf,
	     ((vperm_simulated_ids.v_simulate_root_fs_permissions == 0) ? ",p" : "")) < 0)
		return(NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: packed IDs => '%s'",
		__func__, r);
	return(r);
}

#define GET_SINGLE_ID_GATE(Ret_type, Gatefn_name, Field) \
	Ret_type Gatefn_name(int *result_errno_ptr, \
		Ret_type (*real_fn_ptr)(void), \
		const char *realfnname) \
	{ \
		(void)result_errno_ptr;	/* not used; always succeeds */ \
		(void)real_fn_ptr;	/* real fn is never used directly */ \
		if (vperm_simulated_ids.initialized == 0) \
			initialize_simulated_ids(); \
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %d", realfnname, \
			vperm_simulated_ids.Field); \
		return (vperm_simulated_ids.Field); \
	}

GET_SINGLE_ID_GATE(gid_t, getegid_gate, v_egid)
GET_SINGLE_ID_GATE(uid_t, geteuid_gate, v_euid)
GET_SINGLE_ID_GATE(gid_t, getgid_gate, v_gid)
GET_SINGLE_ID_GATE(uid_t, getuid_gate, v_uid)

/* Note: real getresgid() and getresuid() may return EFAULT, but
 * we can't check that => these simulated ones will always succeed.
*/
#define GET_RES_IDS_GATE(Id_type, Gatefn_name, Field_real, Field_eff, Field_saved) \
	int Gatefn_name(int *result_errno_ptr, \
		int (*real_fn_ptr)(Id_type *r_id_p, Id_type *e_id_p, Id_type *s_id_p), \
		const char *realfnname, \
		Id_type *r_id_p, Id_type *e_id_p, Id_type *s_id_p) \
	{ \
		(void)result_errno_ptr;	/* not used */ \
		(void)real_fn_ptr;	/* real fn is never used directly */ \
		if (vperm_simulated_ids.initialized == 0) \
			initialize_simulated_ids(); \
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %d,%d,%d", realfnname, \
			(int)vperm_simulated_ids.Field_real, \
			(int)vperm_simulated_ids.Field_eff, \
			(int)vperm_simulated_ids.Field_saved); \
		if (r_id_p) *r_id_p = vperm_simulated_ids.Field_real; \
		if (e_id_p) *e_id_p = vperm_simulated_ids.Field_eff; \
		if (s_id_p) *s_id_p = vperm_simulated_ids.Field_saved; \
		return (0); \
	}

GET_RES_IDS_GATE(gid_t, getresgid_gate, v_gid, v_egid, v_saved_gid)
GET_RES_IDS_GATE(uid_t, getresuid_gate, v_uid, v_euid, v_saved_uid)

int setegid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(gid_t gid),
        const char *realfnname,
	gid_t egid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	return(setresgid_gate(result_errno_ptr, NULL/*real_fn_ptr*/,
		realfnname, -1, egid, -1));
}

int seteuid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(uid_t uid),
        const char *realfnname,
	uid_t euid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	return(setresuid_gate(result_errno_ptr, NULL/*real_fn_ptr*/,
		realfnname, -1, euid, -1));
}

#define test_3_id(v, Field1, Field2, Field3) \
	((v==vperm_simulated_ids.Field1) || \
	 (v==vperm_simulated_ids.Field2) || \
	 (v==vperm_simulated_ids.Field3))

#define SET_RES_IDS_GATE(Id_type, Gatefn_name, Field_real, Field_eff, Field_saved, Field_fs) \
	int Gatefn_name(int *result_errno_ptr, \
		int (*real_fn_ptr)(Id_type r_id, Id_type e_id, Id_type s_id), \
		const char *realfnname, \
		Id_type r_id, Id_type e_id, Id_type s_id) \
	{ \
		(void)real_fn_ptr;	/* real fn is never used */ \
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: set (%d,%d,%d)", realfnname, \
			(int)r_id, (int)e_id, (int)s_id); \
		if (vperm_simulated_ids.initialized == 0) \
			initialize_simulated_ids(); \
		if ((vperm_simulated_ids.v_uid == 0) || \
		    (vperm_simulated_ids.v_euid == 0)) { \
			/* Privileged == root */ \
		} else { \
			/* Unprivileged user. Make checks. */ \
			if ((r_id != (Id_type)-1) && \
			    !test_3_id(r_id, Field_real, Field_eff, Field_saved)) \
				goto perm_error; \
			if ((e_id != (Id_type)-1) && \
			    !test_3_id(e_id, Field_real, Field_eff, Field_saved)) \
				goto perm_error; \
			if ((s_id != (Id_type)-1) && \
			    !test_3_id(s_id, Field_real, Field_eff, Field_saved)) \
				goto perm_error; \
		} \
		if (r_id != (Id_type)-1) \
			vperm_simulated_ids.Field_real = r_id; \
		if (e_id != (Id_type)-1) \
			vperm_simulated_ids.Field_eff = e_id; \
		if (s_id != (Id_type)-1) \
			vperm_simulated_ids.Field_saved = s_id; \
		/* manual page states that fs uid is always set. */ \
		vperm_simulated_ids.Field_fs = e_id; \
		return(0); \
	    perm_error: \
		*result_errno_ptr = EPERM; \
		return(-1); \
	} \

SET_RES_IDS_GATE(uid_t, setresuid_gate, v_uid, v_euid, v_saved_uid, v_fsuid)
SET_RES_IDS_GATE(gid_t, setresgid_gate, v_gid, v_egid, v_saved_gid, v_fsgid)

int setuid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(uid_t uid),
        const char *realfnname,
	uid_t uid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();

	if ((vperm_simulated_ids.v_uid == 0) ||
	    (vperm_simulated_ids.v_euid == 0) ||
	    (uid == vperm_simulated_ids.v_uid) ||
	    (uid == vperm_simulated_ids.v_euid) ||
	    (uid == vperm_simulated_ids.v_saved_uid)) {
	    	if (vperm_simulated_ids.v_euid == 0) {
			vperm_simulated_ids.v_uid = uid;
			vperm_simulated_ids.v_saved_uid = uid;
		}
	    	vperm_simulated_ids.v_euid = uid;
	    	vperm_simulated_ids.v_fsuid = uid;
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: set %d", realfnname, uid);
		return(0);
	} /* else no permission to do it. */
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: EPERM", realfnname);
	*result_errno_ptr = EPERM;
	return(-1);
}

int setgid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(gid_t gid),
        const char *realfnname,
	gid_t gid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	if (vperm_simulated_ids.initialized == 0)
		initialize_simulated_ids();

	if ((vperm_simulated_ids.v_uid == 0) ||
	    (vperm_simulated_ids.v_euid == 0) ||
	    (gid == vperm_simulated_ids.v_gid) ||
	    (gid == vperm_simulated_ids.v_egid) ||
	    (gid == vperm_simulated_ids.v_saved_gid)) {
	    	if (vperm_simulated_ids.v_euid == 0) {
			vperm_simulated_ids.v_gid = gid;
			vperm_simulated_ids.v_saved_gid = gid;
		}
	    	vperm_simulated_ids.v_egid = gid;
	    	vperm_simulated_ids.v_fsgid = gid;
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: set %d", realfnname, gid);
		return(0);
	} /* else no permission to do it. */
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: EPERM", realfnname);
	*result_errno_ptr = EPERM;
	return(-1);
}

#define SET_FS_ID_GATE(Id_type, Gatefn_name, Field, \
	Field_real, Field_eff, Field_saved) \
	int Gatefn_name(int *result_errno_ptr, \
		int (*real_fn_ptr)(Id_type fs_id), \
		const char *realfnname, \
		Id_type fs_id) \
	{ \
		(void)result_errno_ptr;	/* not used; always succeeds */ \
		(void)real_fn_ptr;	/* real fn is never used */ \
		if (vperm_simulated_ids.initialized == 0) \
			initialize_simulated_ids(); \
		if ((vperm_simulated_ids.v_uid == 0) || \
		    (vperm_simulated_ids.v_euid == 0) || \
		    (fs_id == vperm_simulated_ids.Field_real) || \
		    (fs_id == vperm_simulated_ids.Field_eff) || \
		    (fs_id == vperm_simulated_ids.Field_saved) || \
		    (fs_id == vperm_simulated_ids.Field)) { \
			vperm_simulated_ids.Field = fs_id; \
		} \
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %d,%d", realfnname, \
			fs_id, vperm_simulated_ids.Field); \
		return (vperm_simulated_ids.Field); \
	}

SET_FS_ID_GATE(gid_t, setfsgid_gate, v_fsgid, v_gid, v_egid, v_saved_gid)
SET_FS_ID_GATE(uid_t, setfsuid_gate, v_fsuid, v_uid, v_euid, v_saved_uid)

int setregid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(gid_t rgid, gid_t egid),
        const char *realfnname, gid_t rgid, gid_t egid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	return(setresgid_gate(result_errno_ptr, NULL/*real_fn_ptr*/,
		realfnname, rgid, egid, -1));
}

int setreuid_gate(int *result_errno_ptr,
	int (*real_fn_ptr)(uid_t ruid, uid_t euid),
        const char *realfnname, uid_t ruid, uid_t euid)
{
	(void)real_fn_ptr;	/* real fn is never used */

	return(setresuid_gate(result_errno_ptr, NULL/*real_fn_ptr*/,
		realfnname, ruid, euid, -1));
}


