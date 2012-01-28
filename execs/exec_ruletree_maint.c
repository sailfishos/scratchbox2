/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Exec rule maintenance routines. */

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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include <sys/mman.h>

#include "sb2_execs.h"

/* "argvmods" rules: */
ruletree_object_offset_t add_exec_preprocessing_rule_to_ruletree(
	const char	*binary_name,
	ruletree_object_offset_t path_prefixes_table_offs,
	ruletree_object_offset_t add_head_table_offs,
	ruletree_object_offset_t add_options_table_offs,
	ruletree_object_offset_t add_tail_table_offs,
	ruletree_object_offset_t remove_table_offs,
	const char *new_filename,
	int disable_mapping)	/* boolean */
{
	ruletree_object_offset_t rule_location = 0;
	ruletree_exec_preprocessing_rule_t new_rule;

	memset(&new_rule, 0, sizeof(new_rule));
	
	if (binary_name) {
		new_rule.rtree_xpr_binary_name_offs = append_string_to_ruletree_file(binary_name);
	}
	new_rule.rtree_xpr_path_prefixes_table_offs = path_prefixes_table_offs;
	new_rule.rtree_xpr_add_head_table_offs = add_head_table_offs;
	new_rule.rtree_xpr_add_options_table_offs = add_options_table_offs;
	new_rule.rtree_xpr_add_tail_table_offs = add_tail_table_offs;
	new_rule.rtree_xpr_remove_table_offs = remove_table_offs;
	if (new_filename) {
		new_rule.rtree_xpr_new_filename_offs = append_string_to_ruletree_file(new_filename);
	}
	new_rule.rtree_xpr_disable_mapping = disable_mapping;

	rule_location = append_struct_to_ruletree_file(&new_rule, sizeof(new_rule),
		SB2_RULETREE_OBJECT_TYPE_EXEC_PP_RULE);
	
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"Added exec preprocessing rule: %s @ %u",
			binary_name, rule_location);

	return(rule_location);
}

