/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Mapping rule maintenance routines. */

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

#include "pathmapping.h"

ruletree_object_offset_t add_rule_to_ruletree(
	const char	*name,
	int		selector_type,
	const char	*selector,
	int		action_type,
	const char	*action_str,
	int		condition_type,
	const char	*condition_str,
	ruletree_object_offset_t rule_list_link,
	int             flags,
	const char      *binary_name,
	int             func_class,
	const char      *exec_policy_name)
{
	ruletree_object_offset_t rule_location = 0;
	ruletree_fsrule_t new_rule;

#if 0	/* FIXME -ADD CHECK */
	if ((ruletree_ctx.rtree_ruletree_fd < 0) &&
	    (open_ruletree(1) < 0)) return(0);
#endif

	memset(&new_rule, 0, sizeof(new_rule));
	
	if (name) {
		new_rule.rtree_fsr_name_offs = append_string_to_ruletree_file(name);
	}

	switch (selector_type) {
	case SB2_RULETREE_FSRULE_SELECTOR_PATH:
	case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
	case SB2_RULETREE_FSRULE_SELECTOR_DIR:
		/* OK */
		break;
	case 0:
		/* OK; it is possible to have a rule without a selector
		 * ("actions" are such cases) */
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Unsupported selector type %d (rule='%s')",
			selector_type, name);
		return(0);
	}

	switch (action_type) {
	case SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE:
	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
	case SB2_RULETREE_FSRULE_ACTION_UNION_DIR:
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
	case SB2_RULETREE_FSRULE_ACTION_SET_PATH:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
	case SB2_RULETREE_FSRULE_ACTION_SUBTREE:
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_MAP_TO:
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_REPLACE_BY:
		/* OK */
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Unsupported action type %d (rule='%s')",
			action_type, name);
		return(0);
	}

	switch (condition_type) {
	case 0: /* it is ok to live without one */
	case SB2_RULETREE_FSRULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS:
	case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE:
	case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE:
	case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY:
	case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_EMPTY:
		/* OK */
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Unsupported condition type %d (rule='%s')",
			action_type, name);
		return(0);
	}


	new_rule.rtree_fsr_rule_list_link = rule_list_link;

	new_rule.rtree_fsr_selector_type = selector_type;
	if (selector) {
		new_rule.rtree_fsr_selector_offs = append_string_to_ruletree_file(selector);
	}

	new_rule.rtree_fsr_action_type = action_type;
	if (action_str) {
		new_rule.rtree_fsr_action_offs = append_string_to_ruletree_file(action_str);
	}

	new_rule.rtree_fsr_condition_type = condition_type;
	if (condition_str) {
		new_rule.rtree_fsr_condition_offs = append_string_to_ruletree_file(condition_str);
	}

	new_rule.rtree_fsr_flags = flags;
	if (binary_name) {
		new_rule.rtree_fsr_binary_name = append_string_to_ruletree_file(binary_name);
	}
	new_rule.rtree_fsr_func_class = func_class;
	if (exec_policy_name) {
		new_rule.rtree_fsr_exec_policy_name = append_string_to_ruletree_file(exec_policy_name);
	}

	rule_location = append_struct_to_ruletree_file(&new_rule, sizeof(new_rule),
		SB2_RULETREE_OBJECT_TYPE_FSRULE);
	
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"Added rule: %d (%s) / %d (%s), @ %u (link=%d)",
			selector_type, (selector ? selector : ""),
			action_type, (action_str ? action_str : ""),
			rule_location, rule_list_link);

	return(rule_location);
}

