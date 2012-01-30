/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Exec preprocessing.
 * ------------------------------------
 *
 * function apply_exec_preprocessing_rules() is called to decide WHAT FILE
 * should be started (see description of the algorithm in sb_exec.c)
 * (this also typically adds, deletes, or modifies arguments whenever needed)
*/

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

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include <sys/mman.h>

#include "sb2_execs.h"

static int add_elements_to_argv(
	const char *table_name,
	ruletree_object_offset_t	add_tbl_offs,
	char	**argv,
	int	add_idx)
{
	int j, n_add;

	if (!add_tbl_offs || !argv) return(add_idx);

	n_add = ruletree_objectlist_get_list_size(add_tbl_offs);

	for (j = 0; j < n_add; j++) {
		const char *str;
		ruletree_object_offset_t str_offs;

		str_offs = ruletree_objectlist_get_item(add_tbl_offs, j);
		str = offset_to_ruletree_string_ptr(str_offs, NULL);
		
		argv[add_idx] = strdup(str);
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s: add from %s to argv[%d] = '%s'",
				__func__, table_name, add_idx, str);
		add_idx++;
	}
	argv[add_idx] = NULL;
	return(add_idx);	/* return index of next free location in argv */
}

static int check_path_prefix_match(
	ruletree_exec_preprocessing_rule_t *execpp_rule,
	const char *filename, const char *file_basename)
{
	ruletree_object_offset_t	prefix_table_offs;
	uint32_t			prefix_table_size;
	int	dirname_len;
	int	i;

	prefix_table_offs = execpp_rule->rtree_xpr_path_prefixes_table_offs;

	if (!prefix_table_offs) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: no path prefixes, assume match",
			__func__);
		return(1);
	}
	dirname_len = file_basename - filename;
	if (!dirname_len) {
		/* relative name, never matches */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: relative filename, no match",
			__func__);
		return(0);
	}
	prefix_table_size = ruletree_objectlist_get_list_size(prefix_table_offs);
	for (i = 0; i < (int)prefix_table_size; i++) {
		ruletree_object_offset_t	prefix_offs;
		const char			*prefix;
		uint32_t			prefix_len;

		prefix_offs = ruletree_objectlist_get_item(prefix_table_offs, i);
		prefix = offset_to_ruletree_string_ptr(
			prefix_offs, &prefix_len);
		if (((int)prefix_len == dirname_len) ||
		    ((int)prefix_len + 1 == dirname_len)) {
			if (!strncmp(filename, prefix, prefix_len)) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"%s: prefix match found",
					__func__);
				return(1);
			}
			SB_LOG(SB_LOGLEVEL_NOISE,
				"%s: didn't match match %s",
				__func__, prefix);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: didn't match match %s (%d,%d)",
				__func__, prefix, prefix_len, dirname_len);
		}
	}
	return(0);
}

static ruletree_exec_preprocessing_rule_t *find_exec_preprocessing_rule(
	ruletree_object_offset_t argvmods_rules_offs,
	const char *filename)
{
	uint32_t list_size = ruletree_objectlist_get_list_size(argvmods_rules_offs);
	const char *file_basename;
	int i;
	ruletree_exec_preprocessing_rule_t *execpp_rule;
	int file_basename_len;

	file_basename = strrchr(filename, '/');
	if (file_basename) {
		file_basename++; /* skip '/' */
	} else {
		/* relative name */
		file_basename = filename;
	}
	file_basename_len = strlen(file_basename);
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: check %d rules, file='%s'",
		__func__, list_size, file_basename);
	for (i = 0; i < (int)list_size; i++) {
		ruletree_object_offset_t r_offs;

		r_offs = ruletree_objectlist_get_item(argvmods_rules_offs, i);
		if (r_offs) {
			execpp_rule = offset_to_exec_preprocessing_rule_ptr(r_offs);
			if (execpp_rule) {
				if (execpp_rule->rtree_xpr_binary_name_offs) {
					const char *rule_bin_name;
					uint32_t rule_bin_name_len;
					
					rule_bin_name = offset_to_ruletree_string_ptr(
						execpp_rule->rtree_xpr_binary_name_offs,
						&rule_bin_name_len);
					SB_LOG(SB_LOGLEVEL_NOISE3,
						"%s: cmp '%s','%s'",
						__func__, file_basename, rule_bin_name);
					if (((int)rule_bin_name_len == file_basename_len) &&
					    !strcmp(file_basename, rule_bin_name)) {

						if (check_path_prefix_match(
							execpp_rule, filename, file_basename)) {
							SB_LOG(SB_LOGLEVEL_DEBUG,
								"%s: Found preprocessing rule '%s'",
								__func__, rule_bin_name);
							return(execpp_rule);
						}
					}
				}
			}
		}
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: No exec preprocessing rules for '%s'.",
		__func__, file_basename);
	return(NULL);
}

int apply_exec_preprocessing_rules(char **file, char ***argv, char ***envp)
{
	static ruletree_object_offset_t	argvmods_rules_offs = 0;
	ruletree_exec_preprocessing_rule_t *execpp_rule;
	int orig_argc;
	int max_new_argv_elements = 0;
	int i = 0;
	char **new_argv = NULL;

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
	if (!argvmods_rules_offs) {
		/* 'argvmods' not found from the tree, DON'T call Lua code */
		return(0);
	}
	execpp_rule = find_exec_preprocessing_rule(
		argvmods_rules_offs, *file);

	if (!execpp_rule) return(0);

	/* Need to do some preprocessing... */

	/* count original number of arguments */
	for (orig_argc=0; (*argv)[orig_argc]; orig_argc++) {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s: orig. argv[%d] = '%s'",
			__func__, orig_argc, (*argv)[orig_argc]);
	}

	if (execpp_rule->rtree_xpr_add_head_table_offs)
		max_new_argv_elements += ruletree_objectlist_get_list_size(
			execpp_rule->rtree_xpr_add_head_table_offs);

	if (execpp_rule->rtree_xpr_add_options_table_offs)
		max_new_argv_elements += ruletree_objectlist_get_list_size(
			execpp_rule->rtree_xpr_add_options_table_offs);

	if (execpp_rule->rtree_xpr_add_tail_table_offs)
		max_new_argv_elements += ruletree_objectlist_get_list_size(
			execpp_rule->rtree_xpr_add_tail_table_offs);

	/* set up argv[] */
	if (max_new_argv_elements > 0) {

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: allocating new argv, size = %d",
			__func__, orig_argc + max_new_argv_elements);
		new_argv = (char **)calloc(orig_argc + max_new_argv_elements + 1, sizeof(char *));
	} else {
		new_argv = *argv;
	}
	i = 0;

	if (execpp_rule->rtree_xpr_add_head_table_offs) {
		i = add_elements_to_argv(
			"add_head",
			execpp_rule->rtree_xpr_add_head_table_offs,
			new_argv, i);
	}

	if (*argv[0]) {
		new_argv[i] = (*argv)[0];
		i++;
	}

	if (execpp_rule->rtree_xpr_add_options_table_offs) {
		i = add_elements_to_argv(
			"add_options",
			execpp_rule->rtree_xpr_add_options_table_offs,
			new_argv, i);
	}

	if (execpp_rule->rtree_xpr_remove_table_offs) {
		ruletree_object_offset_t remove_tbl = execpp_rule->rtree_xpr_remove_table_offs;

		/* remove parameters if needed */
		if (remove_tbl) {
			int remove_table_size = ruletree_objectlist_get_list_size(remove_tbl);
			int j,k;

			/* for every element in orig. argv[1..orig_argc]: */
			for (k = 1; k < orig_argc; k++) {
				int remove_this = 0;
				const char *str;
				for (j = 0; j < remove_table_size; j++) {
					ruletree_object_offset_t str_offs;

					str_offs = ruletree_objectlist_get_item(remove_tbl, j);
					str = offset_to_ruletree_string_ptr(str_offs, NULL);
					if (!strcmp((*argv)[k], str)) {
						remove_this = 1;
						break;
					}
				}
				if (remove_this) {
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"%s: remove argv[%d], '%s'",
						__func__, k, str);
				} else {
					new_argv[i] = strdup((*argv)[k]);
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"%s: argv[%d]='%s'",
						__func__, i, new_argv[i]);
					i++;
				}
			}
		}
	} else {
		int k;
		/* nothing to remove, copy old argv */
		for (k = 1; k < orig_argc; i++, k++) {
			new_argv[i] = strdup((*argv)[k]);
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: move argv[%d] -> argv[%d], '%s'",
				__func__, k, i, new_argv[i]);
		}
	}
	new_argv[i] = NULL;

	if (execpp_rule->rtree_xpr_add_tail_table_offs) {
		i = add_elements_to_argv(
			"add_tail",
			execpp_rule->rtree_xpr_add_tail_table_offs,
			new_argv, i);
	}

	/* replace orig. argv */
	if (*argv != new_argv) {
		/* FIXME: free the old vector. This leaks memory,
		 * but since this is preparing for exec,
		 * the problem will disappear soon anyway..
		*/
		*argv = new_argv;
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: argv (new_argc=%d) was replaced.", __func__, i);
	}

	if (execpp_rule->rtree_xpr_new_filename_offs) {
		const char *new_file_name;
		new_file_name = offset_to_ruletree_string_ptr(
			execpp_rule->rtree_xpr_new_filename_offs, NULL);
		if (new_file_name) {
#if 0 /* FIXME */
			if (*file) free(*file);
#endif
			*file = strdup(new_file_name);
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: new filename '%s'.",
				__func__, new_file_name);
			(*argv)[0] = strdup(*file);
		}
	}

	if (execpp_rule->rtree_xpr_disable_mapping) {
		int orig_envc;
		char **new_envp;
		int k;

		for (orig_envc=0; (*envp)[orig_envc]; orig_envc++);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: num.env.vars = %d, allocate +2",
			__func__, orig_envc);
		new_envp = (char **)calloc(orig_envc + 2 + 1, sizeof(char *));
		/* copy old env. */
		for (k = 0; k < orig_envc; k++) {
			new_envp[k] = strdup((*envp)[k]);
		}
		new_envp[orig_envc] = strdup("SBOX_DISABLE_MAPPING=1");
		new_envp[orig_envc+1] = strdup("SBOX_DISABLE_ARGVENVP=1");
		new_envp[orig_envc+2] = NULL;
		*envp = new_envp;
	}

	return(0);
}

