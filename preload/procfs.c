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
	if (env == NULL)
		return(NULL);
	else
		return(strchr(env, '=') + 1);
}

/* check (and create if it doesn't exits) the symlink which is used to
 * replace /proc/self/exe or /proc/<pid>/exe
 *
 * [Note that this code may be called from several threads or even several
 * processes in parallel, but because the resulting link is always the same,
 * this doesn't matter!]
*/
static char *symlink_for_exe_path(
	char *pathbuf, size_t bufsize, char *exe_path, pid_t pid)
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
		    pathbuf, sizeof(pathbuf), exe_path_inside_sb2, getpid())) {
			return(strdup(pathbuf));
		}
		/* oops, failed to create the replacement.
		 * must use the real link, it points to wrong place.. */
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
                char    *exe_path_inside_sb2;
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
		free(buffer);

		orig_binary_name = read_env_value(orig_binary_name);
		real_binary_name = read_env_value(real_binary_name);

                /* Try to use the unmapped path (..orig..) if
                 * possible, otherwise use the mapped path
		 */
                exe_path_inside_sb2 = orig_binary_name ?
			(char *)orig_binary_name : (char *)real_binary_name;

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
			return(NULL);
		}
		/* must create a replacement: */
		if (symlink_for_exe_path(
		    pathbuf, sizeof(pathbuf), exe_path_inside_sb2, pid)) {
			return(strdup(pathbuf));
		}
		/* oops, failed to create the replacement.
                 * must use the real link, it points to wrong place.. 
		 */
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

