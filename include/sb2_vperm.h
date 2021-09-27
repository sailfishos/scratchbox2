/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef __SB2VPERM_H
#define __SB2VPERM_H

#include <unistd.h>
#include <sys/types.h>

extern uid_t vperm_getuid(void);
extern uid_t vperm_geteuid(void);
extern uid_t vperm_get_real_euid(void);
extern gid_t vperm_getegid(void);

extern int vperm_uid_or_gid_virtualization_is_active(void);

extern char *vperm_export_ids_as_string_for_exec(const char *prefix,
	int mode, uid_t suid, gid_t sgid, const char *user_vperm_request);

extern int vperm_set_owner_and_group_of_unknown_files(uid_t *uidp, gid_t *gidp);

extern int vperm_simulate_root_fs_permissions(void);

#endif
