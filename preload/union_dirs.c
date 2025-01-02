/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2010,2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <config.h>

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
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

#include <limits.h>
#include <sys/param.h>
#include <sys/file.h>
#include <assert.h>

#include "mapping.h"
#include "sb2.h"
#include "rule_tree.h"
#include "libsb2.h"
#include "exported.h"


/* returns an allocated string */
/* FIXME: This should be somewhere else! it does not belong to this file. */
char *prep_union_dir(const char *dst_path, const char **src_paths, int num_real_dir_entries)
{
	DIR *d = NULL;
	struct dirent *de;
	int count = 0;
	int i;
	char *udir_name;
	char *cp;
	int slash_count = 0;
	char *mod_dst_path = NULL;
	char *result_path;

	if (num_real_dir_entries < 1) goto error_out;

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"prep_union_dir: dst=%s #%d source directories",
		dst_path, num_real_dir_entries);

	/* add number of slashes to the path. this makes it possible
	 * to have union directories that have other union directories
	 * as subdirectories (because the names in the directories
	 * always refer to ordinary, empty files).
	*/
	mod_dst_path = strdup(dst_path);
	cp = mod_dst_path;
	while(*cp) {
		if (*cp == '/') slash_count++;
		cp++;
	}
	if (asprintf(&udir_name, "%s/uniondirs/%d", sbox_session_dir, slash_count) < 0)
		goto asprint_failed_error_out;
	SB_LOG(SB_LOGLEVEL_DEBUG, "prep_union_dir: mkdir(%s)", udir_name);
	mkdir_nomap_nolog(udir_name, 0700);
	free(udir_name);
	
	/* this is same as mkdir -p, effectively */
	cp = mod_dst_path;
	while(*cp == '/') *cp++ = '@'; /* replace leading slashes */
	do {
		if(cp && *cp) {
			cp = strchr(cp, '/');
			if (cp) *cp = '\0'; /* temporarily terminate the string here */
		}
		if (asprintf(&udir_name, "%s/uniondirs/%d/%s",
			 sbox_session_dir, slash_count, mod_dst_path) < 0)
				goto asprint_failed_error_out;
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"prep_union_dir: mkdir(%s)", udir_name);
		mkdir_nomap_nolog(udir_name, 0700);
		free(udir_name);
		if (cp) *cp++ = '/'; /* restore the slash, if there is more */
	} while(cp);

	for (i = 0; i < num_real_dir_entries; i++) {
		const char *src_path = src_paths[i];

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"prep_union_dir: src dir '%s'", src_path);

		if ( (d = opendir_nomap_nolog(src_path)) == NULL )
			continue;

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"prep_union_dir: opened src dir '%s'", src_path);

		while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
			int fd;
			char *tmp_name;

			if (de->d_name[0] == '.') {
				if (de->d_name[1] == '\0') continue;
				if ((de->d_name[1] == '.') &&
				    (de->d_name[2] == '\0')) continue;
			}
			if (asprintf(&tmp_name, "%s/uniondirs/%d/%s/%s",
				sbox_session_dir, slash_count,
				mod_dst_path, de->d_name) < 0) {
					closedir(d);
					goto asprint_failed_error_out;
				}
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"prep_union_dir: tmp=%s", tmp_name);

			fd = creat_nomap(tmp_name, 0644);
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"prep_union_dir: fd=%d", fd);
			close(fd);

			free(tmp_name);
			count++;
		}
		closedir(d);
	}
	if (!count) goto error_out;

	if (asprintf(&result_path, "%s/uniondirs/%d/%s",
		sbox_session_dir, slash_count, mod_dst_path) < 0)
			goto asprint_failed_error_out;
	free(mod_dst_path);
	return result_path;

    asprint_failed_error_out:
	SB_LOG(SB_LOGLEVEL_ERROR, "asprintf failed to allocate memory");
    error_out:
	if(mod_dst_path) free(mod_dst_path);
	return NULL;
}

