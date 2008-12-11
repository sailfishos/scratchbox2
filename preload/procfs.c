/*
 * Copyright (c) 2008 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * /proc simulation for SB2.
 *
 * /proc/self/exe (as well as /proc/<MY_PID>/exe) needs special care
 * if the binary was started by anything else than direct exec:
 *    a) if CPU transparency is used, "exe" may point to e.g. Qemu
 *    b) if "ld.so-start" was used, "exe" points to ld.so and not
 *       to the binary itself.
 *
 * (all other files under /proc are used directly)
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <mapping.h>
#include <sb2.h>
#include "libsb2.h"
#include "exported.h"

/* check (and create if it doesn't exits) the symlink which is used to
 * replace /proc/sef/exe
 *
 * [Note that this code may be called from several threads or even several
 * processes in parallel, but because the resulting link is always the same,
 * this doesn't matter!]
*/
static char *symlink_for_exe_path(char *pathbuf, size_t bufsize, char *exe_path)
{
	int	depth;
	char	*cp;
	int	prefixlen;

	/* first count number of slashes in exe_path */
	for (cp=exe_path, depth=0; *cp; cp++) {
		if (*cp == '/') {
			depth++;
			while (cp[1] == '/') cp++;
		}
	}
	/* ensure that the directory for links with "depth" levels exists: */
	prefixlen = snprintf(pathbuf, bufsize, "%s/proc/X.%d",
		sbox_session_dir, depth);
	if ((prefixlen + 2 + strlen(exe_path)) >= bufsize) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Can't create replacement for /proc/self/exe; "
			"resulting path is too long");
		return(NULL);
	}
	mkdir_nomap_nolog(pathbuf, 0755);

	for (cp=exe_path; cp && *cp; ) {
		char	*next_slash = strchr(cp+1, '/');

		/* This algorithm is not too efficient (first copies
		 * everything, then cuts it), but usually "exe_path"
		 * is really short, so usually it doesn't matter..
		*/
		strcat(pathbuf, cp); /* Always fits; We already checked that */
		if (next_slash) {
			pathbuf[prefixlen + (next_slash - exe_path)] = '\0';
			SB_LOG(SB_LOGLEVEL_NOISE, "Create DIR '%s'", pathbuf);
			mkdir_nomap_nolog(pathbuf, 0755);
			cp = next_slash;
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE, "Create SYMLINK '%s' => '%s'",
				pathbuf, exe_path);
			symlink_nomap_nolog(exe_path, pathbuf);
			cp = NULL;
		}
	}
	return(pathbuf);
}

static char *procfs_mapping_request_for_my_files(
	char *full_path, char *base_path)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request_for_my_files(%s)",
		full_path);

	if (!strcmp(base_path,"exe")) {
		char	*exe_path_inside_sb2;
		char	pathbuf[PATH_MAX];
		char    link_dest[PATH_MAX+1];
		int	link_len;

		/* Try to use the unmapped path (..orig..) if
		 * possible, otherwise use the mapped path
		*/
		exe_path_inside_sb2 = sbox_orig_binary_name ?
			sbox_orig_binary_name : sbox_real_binary_name;

		/* check if the real link is OK: */
		link_len = readlink_nomap(full_path, link_dest, PATH_MAX);
		if ((link_len > 0) &&
		    !strcmp(exe_path_inside_sb2, link_dest)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"procfs_mapping_request_for_my_files:"
				" real link is ok (%s,%s)",
				full_path, link_dest);
			return(NULL);
		}
		/* must create a replacement: */
		if (symlink_for_exe_path(
		     pathbuf, sizeof(pathbuf), exe_path_inside_sb2)) {
			return(strdup(pathbuf));
		}
		/* oops, failed to create the replacement.
		 * must use the real link, it points to wrong place.. */
		return(NULL);
	}
	return(NULL);
}

static const char proc_self_path[] = "/proc/self/";

/* procfs_mapping_request(): Called from luaif.c
 * returns mapped path if the path needs mapping,
 * or NULL if the original path can be used directly.
*/
char *procfs_mapping_request(char *path)
{
	char	my_process_path[PATH_MAX];
	int	len;

	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request(%s)", path);

	if (!strncmp(path, proc_self_path, sizeof(proc_self_path)-1))
		return(procfs_mapping_request_for_my_files(path,
			path+sizeof(proc_self_path)-1));

	len = snprintf(my_process_path, sizeof(my_process_path), "/proc/%u/",
		(unsigned)getpid());
	if (!strncmp(path, my_process_path, len))
		return(procfs_mapping_request_for_my_files(path, path+len));

	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request: not mapped");
	return(NULL);
}

