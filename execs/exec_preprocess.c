/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Exec preprocessing. */

#if 0
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include <sys/mman.h>

#include "sb2_execs.h"

int apply_exec_preprocessing_rules(char **file, char ***argv, char ***envp)
{
	static ruletree_object_offset_t	argvmods_rules_offs = 0;

	if (!*file || !**file) return(0); /* file is required. */

	if (!argvmods_rules_offs) {
		const char *modename = sbox_session_mode;
		uint32_t   use_gcc_rules = 0;

		if (!modename)
			modename = ruletree_catalog_get_string("MODES", "#default");
		if (modename) {
			uint32_t   *need_gcc_rules_p;

			need_gcc_rules_p = ruletree_catalog_get_boolean_ptr(
				"use_gcc_argvmods", modename);
			if (*need_gcc_rules_p) use_gcc_rules = 1;

			argvmods_rules_offs = ruletree_catalog_get("argvmods",
				(use_gcc_rules ? "gcc" : "misc"));

			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: argvmods rules @%u, use_gcc_rules=%d",
				__func__, argvmods_rules_offs, use_gcc_rules);
                }
	}
	if (argvmods_rules_offs) {
		/* argvmods_rules_offs => object list */
		uint32_t list_size = ruletree_objectlist_get_list_size(argvmods_rules_offs);
		const char *file_basename;
		int i;
		ruletree_object_offset_t execpp_rule_offs = 0;

		file_basename = strrchr(*file, '/');
		if (file_basename) {
			file_basename++; /* skip '/' */
		} else {
			/* relative name */
			file_basename = *file;
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: check %d rules, file='%s'",
			__func__, list_size, file_basename);
		for (i = 0; i < list_size; i++) {
			ruletree_object_offset_t r_offs;
	
			r_offs = ruletree_objectlist_get_item(argvmods_rules_offs, i);
			if (r_offs) {
				ruletree_exec_preprocessing_rule_t *rule;

				rule = offset_to_exec_preprocessing_rule_ptr(r_offs);
				if (rule) {
					if (rule->rtree_xpr_binary_name_offs) {
						const char *rule_bin_name;
						rule_bin_name = offset_to_ruletree_string_ptr(
							rule->rtree_xpr_binary_name_offs, NULL);
						SB_LOG(SB_LOGLEVEL_NOISE3,
							"%s: cmp '%s','%s'",
							__func__, file_basename, rule_bin_name);
						if (!strcmp(file_basename, rule_bin_name)) {
							SB_LOG(SB_LOGLEVEL_DEBUG,
								"%s: Found preprocessing rule '%s'",
								__func__, rule_bin_name);
							execpp_rule_offs = r_offs;
							break;
						}
					}
				}
			}
		}
		if (!execpp_rule_offs) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: No exec preprocessing rules for '%s'.",
				__func__, file_basename);
			return(0);
		}
		/* Need to do some preprocessing... */

		/* FIXME: This implementation uses the rule tree only to
		 * find out if preprocessing is needed; it it is, then
		 * Lua code is called to do the processing.
		 * The rules are already in the tree, but not yet used...
		*/
	}

	/* 'argvmods' not found from the tree, call Lua code */
	return(sb_execve_preprocess(file, argv, envp));
}
