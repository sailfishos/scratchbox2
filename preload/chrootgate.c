/*
 * libsb2 -- chroot() simulation.
 *
 * Copyright (C) 2012 Nokia Corporation.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "sb2.h"
#include "sb2_stat.h"
#include "libsb2.h"
#include "exported.h"

int chroot_gate(int *result_errno_ptr,
	int (*real_chroot_ptr)(const char *path),
        const char *realfnname,
	const char *path)
{
	char	*new_chroot_path = NULL;
	char	*cp = NULL;
	struct stat64	statbuf;
	mapping_results_t mapped_chroot_path;

	(void)real_chroot_ptr; /* won't call the real function ever */

	clear_mapping_results_struct(&mapped_chroot_path);

	/* Parameter 'path' may not be a clean path,
	 * and it isn't necessarily even absolute path. */
	if (*path == '/' && sbox_chroot_path) {
		char	*path2 = NULL;
		/* "path" is absolute == really it is relative to
		 * sbox_chroot_path if that is set; otherwise it is
		 * relative to the virtual root */

		if (asprintf(&path2, "%s/%s", sbox_chroot_path, path) < 0) {
			*result_errno_ptr = EIO;
				return(-1);
		}
		SB_LOG(SB_LOGLEVEL_DEBUG, "chroot, old dir='%s'",
		       sbox_chroot_path);

		/* sbox_virtual_path_to_abs_virtual_path() will clean it */
		new_chroot_path = sbox_virtual_path_to_abs_virtual_path(
			sbox_binary_name, realfnname, SB2_INTERFACE_CLASS_CHROOT,
			path2, result_errno_ptr);
	} else {
		/* "path" is relative to CWD. */
		new_chroot_path = sbox_virtual_path_to_abs_virtual_path(
			sbox_binary_name, realfnname, SB2_INTERFACE_CLASS_CHROOT,
			path, result_errno_ptr);
	}

	if (new_chroot_path == NULL) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"chroot: unable to make virtual path absolute: %s",
			path);
		goto free_mapping_results_and_return_minus1;
	}

	if (!strcmp(new_chroot_path, "/")) {
		/* No need for chrooting, new_chroot_path == "/"
		 * dectivate chroot simulation if it was in use
		 * (e.g. "rpm" does just that:
		 *   1) chdir("/")
		 *   2) chroot(path_to_sysroot)
		 *   3) chroot(".") to get out from the chroot.
		 * N.B. "/" exists always, no need to check it.
		*/
		if(sbox_chroot_path) {
			SB_LOG(SB_LOGLEVEL_INFO, "deactivating chroot (sbox_chroot_path was '%s')",
			sbox_chroot_path);
			cp = sbox_chroot_path;
			sbox_chroot_path = NULL;
			free(cp);
		}
		free(new_chroot_path);
	} else {
		/* activate chroot simulation.
		 * First test that "path" is a valid location:
		 * new_chroot_path is an absolute virtual path, but relative to
		 * the virtual "real" root. If chrooting is already active,
		 * it would be mapped twice if it was used here. So,
		 * start by mapping "path": */
		sbox_map_path(realfnname, path, 0, &mapped_chroot_path,
			SB2_INTERFACE_CLASS_CHROOT);
		if (mapped_chroot_path.mres_errno) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "chroot: pathmapping failed");
			*result_errno_ptr = EPERM; /* well, not really... */
			goto free_mapping_results_and_return_minus1;
		}
		/* check if the target exists. */
		SB_LOG(SB_LOGLEVEL_DEBUG, "chroot: testing '%s'",
			mapped_chroot_path.mres_result_path);
		if (real_stat64(mapped_chroot_path.mres_result_path, &statbuf) < 0) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "chroot: failed to stat the destination dir (%s)",
				mapped_chroot_path.mres_result_path);
			*result_errno_ptr = ENOENT;
			goto free_mapping_results_and_return_minus1;
		}
		/* it must be a directory. */
		if (!S_ISDIR(statbuf.st_mode)) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "chroot: destination is not a directory (%s)",
				mapped_chroot_path.mres_result_path);
			*result_errno_ptr = ENOTDIR;
			goto free_mapping_results_and_return_minus1;
		}
		/* free mapping results, the virtual path is enough after this.*/
		free_mapping_results(&mapped_chroot_path);

		SB_LOG(SB_LOGLEVEL_INFO, "chroot '%s' (new abs.virtual chroot path='%s')",
			path, new_chroot_path);

		cp = sbox_chroot_path;
		sbox_chroot_path = new_chroot_path;
		if (cp) free(cp);
	}
	return(0);

    free_mapping_results_and_return_minus1:
	free_mapping_results(&mapped_chroot_path);
	return(-1);
}

