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
 * /proc/self/exe (as well as /proc/<MY_PID>/exe and /proc/<PID>L/exe)
 * needs special care if the binary was started by anything else
 * than direct exec:
 *    a) if CPU transparency is used, "exe" may point to e.g. Qemu
 *    b) if "ld.so-start" was used, "exe" points to ld.so and not
 *       to the binary itself.
 *
 * (all other files under /proc are used directly)
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

#include <mapping.h>
#include <sb2.h>
#include "libsb2.h"
#include "exported.h"

#define BLOCK_SIZE (4*1024)

/*
 * We use dynamic buffer because size of the files in procfs
 * cannot been checked by stat() or lseek()
 * Note that caller must free a buffer if the function returns
 * other than zero.
*/
static size_t read_procfs_file_to_buffer(const char *path, char **buffer)
{
	int fd;
	char *buf;
	ssize_t rc = 0, total = 0;
	size_t count = BLOCK_SIZE;

	fd = open_nomap(path, O_RDONLY);
	if (fd < 0) {
		return(0);
	}

	buf = malloc(count);
	if (!buf) {
		close(fd);
		return(0);
	}

	for (;;) {
		rc = read(fd, buf + total, BLOCK_SIZE);
		if (rc == 0) {
			/* eof */
			break;
		}
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				free(buf);
				close(fd);
				return(0);
			}
		}

		if (rc == BLOCK_SIZE) {
			count += BLOCK_SIZE;
			buf = realloc(buf, count);
			if (!buf) {
				close(fd);
				return(0);
			}
		}
		total += rc;
	}

	close(fd);
	*buffer = buf;

	return (total);
}

/* different environment varibles are `\0`separated */
static const char *read_env_variable(
	const char *name, const char *buf, size_t len)
{
        size_t name_len = strlen(name);
        size_t l, i = 0;

        while (i < len) {
                l = strlen(buf + i);
                if (l > 0) {
                        if (strncmp(buf + i, name, name_len) == 0) {
				return(strdup(buf + i));
                        }
                }
                i += l + 1;
        }
        return(NULL);
}

/* name=value'\0' */
static const char *read_env_value(const char *env)
{
	const char *cp;

	if (env == NULL) return(NULL);

	cp = strchr(env, '=');
	if (cp) return(cp + 1);
	return(NULL);
}

/* check (and create if it doesn't exits) the symlink which is used to
 * replace /proc/self/exe or /proc/<pid>/exe
 *
 * [Note that this code may be called from several threads or even several
 * processes in parallel, but because the resulting link is always the same,
 * this doesn't matter!]
*/
static char *symlink_for_exe_path(
	char *pathbuf, size_t bufsize, const char *exe_path, pid_t pid)
{
	int		depth;
	const char	*cp;
	int		prefixlen;

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
			"Can't create replacement for /proc/%u/exe; "
		        "resulting path is too long", pid);
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

/* Determine best possible path specification for executable.
 *
 * Returns string in newly allocated buffer, caller should deallocate it.
 */
static char *select_exe_path_for_sb2(
	const char *orig_binary_name, const char *real_binary_name)
{
	if (real_binary_name) {
		/* real_binary_name contains host real path */
		/* try to determine virtal real path */
		char *exe = scratchbox_reverse_path("select_exe_path_for_sb2", real_binary_name,
				SB2_INTERFACE_CLASS_PROC_FS_OP);
		if (exe)
			return(exe);
		return(strdup(real_binary_name));
	}

	if (orig_binary_name) {
		/* only unmapped, unclean path is available */
		char *rp;
		char *reverse;
		struct sb2context *sb2if = get_sb2context();

		/* lua mapping is disabled at this point, need to enable it */
		enable_mapping(sb2if);
		/* calculate host real path */
		rp = canonicalize_file_name(real_binary_name);
		disable_mapping(sb2if);
		release_sb2context(sb2if);

		if (!rp) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"select_exe_path_for_sb2(%s, %s):"
				" error while determining real path: %s",
				orig_binary_name,
				real_binary_name,
				strerror(errno));
			return(NULL);
		}

		reverse = scratchbox_reverse_path("select_exe_path_for_sb2", rp,
				SB2_INTERFACE_CLASS_PROC_FS_OP);
		if (reverse) {
			free(rp);
			rp = reverse;
		}
		return(rp);
	}

	SB_LOG(SB_LOGLEVEL_DEBUG,
	       "procfs_mapping failed to get absolute exe path");
	return(NULL);
}

static char *procfs_mapping_request_for_my_files(
	char *full_path, char *base_path)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request_for_my_files(%s)",
		full_path);

	if (!strcmp(base_path,"exe")) {
		const char *exe_path_inside_sb2;
		char	pathbuf[PATH_MAX];
		char    link_dest[PATH_MAX+1];
		int	link_len;

                exe_path_inside_sb2 = select_exe_path_for_sb2(
			sbox_orig_binary_name, sbox_real_binary_name);

                if (!exe_path_inside_sb2) return(NULL);

		/* check if the real link is OK: */
		link_len = readlink_nomap(full_path, link_dest, PATH_MAX);
		if ((link_len > 0) &&
		    !strcmp(exe_path_inside_sb2, link_dest)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"procfs_mapping_request_for_my_files:"
				" real link is ok (%s,%s)",
				full_path, link_dest);
			free((void*)exe_path_inside_sb2);
			return(NULL);
		}
		/* must create a replacement: */
		if (symlink_for_exe_path(
		    pathbuf, sizeof(pathbuf), exe_path_inside_sb2, getpid())) {
			free((void*)exe_path_inside_sb2);
			return(strdup(pathbuf));
		}
		/* oops, failed to create the replacement.
		 * must use the real link, it points to wrong place.. */
		free((void*)exe_path_inside_sb2);
		return(NULL);
	}
	return(NULL);
}

static char *procfs_mapping_request_for_other_files(
        char *full_path, char *base_path, char *pid_path, pid_t pid)
{
        SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request_for_other_files(%s)",
	       full_path);

        if (!strcmp(base_path,"exe")) {
                const char *exe_path_inside_sb2;
                char    *buffer;
                char    pathbuf[PATH_MAX];
                char    link_dest[PATH_MAX+1];
                int     link_len;
                size_t  len;
                const char *orig_binary_name;
                const char *real_binary_name;

                /* Check the process environment to find out is this 
		 * runned under sb2 
		 */
                (void)snprintf(pathbuf, sizeof(pathbuf), "%s/environ",
			       pid_path);
                len = read_procfs_file_to_buffer(pathbuf, &buffer);
                if (len == 0) {
                        return(NULL);
                }

                orig_binary_name = read_env_variable("__SB2_ORIG_BINARYNAME",
                                                  buffer, len);
                real_binary_name = read_env_variable("__SB2_REAL_BINARYNAME",
                                                  buffer, len);

		/* we don't need buffer anymore */
		free((void*)buffer);

		orig_binary_name = read_env_value(orig_binary_name);
		real_binary_name = read_env_value(real_binary_name);

                exe_path_inside_sb2 = select_exe_path_for_sb2(
			orig_binary_name, real_binary_name);
		
                /* this is not under runned binary */
                if (!exe_path_inside_sb2) {
                        return(NULL);
                }

                /* check if the real link is OK: */
                link_len = readlink_nomap(full_path, link_dest, PATH_MAX);
                if ((link_len > 0) &&
                    !strcmp(exe_path_inside_sb2, link_dest)) {
                        SB_LOG(SB_LOGLEVEL_DEBUG,
			       "procfs_mapping_request_for_other_files:"
			       " real link is ok (%s,%s)",
			       full_path, link_dest);
			free((void*)exe_path_inside_sb2);
			return(NULL);
		}
		/* must create a replacement: */
		if (symlink_for_exe_path(
		    pathbuf, sizeof(pathbuf), exe_path_inside_sb2, pid)) {
			free((void*)exe_path_inside_sb2);
			return(strdup(pathbuf));
		}
		/* oops, failed to create the replacement.
                 * must use the real link, it points to wrong place.. 
		 */
		free((void*)exe_path_inside_sb2);
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
	int	len, count;
	pid_t   pid;

	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request(%s)", path);

	if (!strncmp(path, proc_self_path, sizeof(proc_self_path)-1))
		return(procfs_mapping_request_for_my_files(path,
			path+sizeof(proc_self_path)-1));

	len = snprintf(my_process_path, sizeof(my_process_path), "/proc/%u/",
		(unsigned)getpid());
	if (!strncmp(path, my_process_path, len))
		return(procfs_mapping_request_for_my_files(path, path+len));

	/* some other /proc/<pid>/exe */
	count = sscanf(path, "/proc/%u/", &pid);
	if (count != 1)
		return(NULL);

	len = snprintf(my_process_path, sizeof(my_process_path), "/proc/%u/",
		       (unsigned)pid);

	if (!strncmp(path, my_process_path, len))
		return(procfs_mapping_request_for_other_files(
			       path, path+len, my_process_path, pid));

	SB_LOG(SB_LOGLEVEL_DEBUG, "procfs_mapping_request: not mapped");
	return(NULL);
}

