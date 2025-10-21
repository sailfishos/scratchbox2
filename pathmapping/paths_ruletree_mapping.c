/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Mapping rule execution routines. */

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
#include "processclock.h"


/* Returns min.path length if a match is found, otherwise returns -1 */
static int ruletree_test_path_match(const char *full_path, size_t full_path_len, ruletree_fsrule_t *rp)
{
	const char	*selector = NULL;
	const char	*match_type = "no match";
	int		result = -1;
	uint32_t	selector_len;

	if (!rp || !full_path) {
		SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match fails");
		return(-1);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match (%s), type=%d",
		full_path, rp->rtree_fsr_selector_type);

	selector = offset_to_ruletree_string_ptr(rp->rtree_fsr_selector_offs,
		&selector_len);

	switch (rp->rtree_fsr_selector_type) {
	case SB2_RULETREE_FSRULE_SELECTOR_PATH:
		if (selector) {
			if ((selector_len == full_path_len) &&
			    !strcmp(full_path, selector)) {
				result = selector_len;
				match_type = "path";
			}
		}
		break;
	case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
		if (selector && (*selector != '\0')) {
			if ((full_path_len >= selector_len) &&
			    (full_path[selector_len-1] == selector[selector_len-1]) &&
			    !strncmp(full_path, selector, selector_len)) {
				result = selector_len;
				match_type = "prefix";
			}
		}
		break;
	case SB2_RULETREE_FSRULE_SELECTOR_DIR:
		if (selector && (*selector != '\0')) {
			/* test a directory prefix: the next char after the
			 * prefix must be '\0' or '/', unless we are accessing
			 * the root directory */
			if ((full_path_len >= selector_len) &&
			    ((full_path[selector_len] == '/') ||
			     (full_path[selector_len] == '\0') ||
			     ((selector_len == 1) && (*full_path=='/'))) ) {
				if (!strncmp(full_path, selector, selector_len)) {
					result = selector_len;
					match_type = "dir";
				}
			}
		}
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_test_path_match: "
			"Unsupported selector type (rule='%s')",
			offset_to_ruletree_string_ptr(rp->rtree_fsr_name_offs, NULL));
		return(-1);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"ruletree_test_path_match '%s' (%lu), '%s' (%u) => %d (%s)",
		full_path, full_path_len, selector, selector_len, result, match_type);
	return(result);
}

static ruletree_object_offset_t ruletree_find_rule(
        const path_mapping_context_t *ctx,
	ruletree_object_offset_t rule_list_offs,
	const char *virtual_path,
	size_t virtual_path_len,
	int *min_path_lenp,
	uint32_t fn_class,
	ruletree_fsrule_t	**rule_p)
{
	uint32_t	rule_list_size;
	uint32_t	i;
	PROCESSCLOCK(clk1)

	START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "ruletree_find_rule");
	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_find_rule for (%s)", virtual_path);
	rule_list_size = ruletree_objectlist_get_list_size(rule_list_offs);

	if (min_path_lenp) *min_path_lenp = 0;
	if (rule_p) *rule_p = NULL;

	if (rule_list_size == 0) return(0);

	for (i = 0; i < rule_list_size; i++) {
		ruletree_fsrule_t	*rp;
		ruletree_object_offset_t rule_offs;

		rule_offs = ruletree_objectlist_get_item(rule_list_offs, i);
		if (!rule_offs) continue;

		rp = offset_to_ruletree_fsrule_ptr(rule_offs);
		if (rp) {
			int min_path_len;

			if (rp->rtree_fsr_condition_type != 0) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"ruletree_find_rule: can't handle rules with conditions, fail. @%d", rule_offs);
				return(0);
			}

			if (rp->rtree_fsr_selector_type == 0) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"ruletree_find_rule skipping defunct rule @%d", rule_offs);
				continue;
			}

			min_path_len = ruletree_test_path_match(virtual_path, virtual_path_len, rp);

			if (min_path_len >= 0) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"ruletree_find_rule found rule @ %d",
					rule_offs);

				if (rp->rtree_fsr_func_class) {
					if ((rp->rtree_fsr_func_class & fn_class) == 0) {
						/* Function class does not match.. */
						continue;
					}
				}

				if (rp->rtree_fsr_binary_name) {
					const char	*bin_name_in_rule =
						offset_to_ruletree_string_ptr(rp->rtree_fsr_binary_name, NULL);
					if (strcmp(ctx->pmc_binary_name, bin_name_in_rule)) {
						/* binary name does not match, not this rule... */
						continue;
					}
				}

				if (min_path_lenp) *min_path_lenp = min_path_len;

				if (rp->rtree_fsr_action_type == SB2_RULETREE_FSRULE_ACTION_SUBTREE) {
					/* if rule can be found from the subtree, return it,
                                         * otherwise continue looping here */
					if (rp->rtree_fsr_rule_list_link) {
						ruletree_object_offset_t subtree_offs;

						SB_LOG(SB_LOGLEVEL_NOISE,
							"ruletree_find_rule: continue @ %d",
							rp->rtree_fsr_rule_list_link);
						subtree_offs = ruletree_find_rule(ctx,
							rp->rtree_fsr_rule_list_link,
							virtual_path, virtual_path_len,
							min_path_lenp,
							fn_class, rule_p);
						if (subtree_offs) {
							STOP_AND_REPORT_PROCESSCLOCK(
								SB_LOGLEVEL_INFO, &clk1,
								"found/subtree");
							return(subtree_offs);
						}
					} else {
						SB_LOG(SB_LOGLEVEL_NOISE,
							"ruletree_find_rule: no link");
					}
					continue;
				}
				/* found it! */
				if (rule_p) *rule_p = rp;
				STOP_AND_REPORT_PROCESSCLOCK(
					SB_LOGLEVEL_INFO, &clk1,
					"found");
				return(rule_offs);
			}
		}
	}
	STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "not found");
	return (0); /* failed to find it */
}

ruletree_object_offset_t ruletree_get_rule_list_offs(
	int use_fwd_rules, const char **errormsgp)
{
	static ruletree_object_offset_t fwd_rule_list_offs = 0;
	static ruletree_object_offset_t rev_rule_list_offs = 0;

	if (!fwd_rule_list_offs || !rev_rule_list_offs) {
		const char *modename = sbox_session_mode;

		if (!modename)
			modename = ruletree_catalog_get_string("MODES", "#default");
		if (!modename) {
			*errormsgp = "No default modename!";
			return(0);
		}
		fwd_rule_list_offs = ruletree_catalog_get("fs_rules", modename);
		rev_rule_list_offs = ruletree_catalog_get("rev_rules", modename);
		if (!fwd_rule_list_offs) {
			*errormsgp = "No rules found from ruletree!";
			return(0);
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: rule list locations: fwd @%d, rev @%d",
			__func__, fwd_rule_list_offs, rev_rule_list_offs);
	}
	return(use_fwd_rules ? fwd_rule_list_offs : rev_rule_list_offs);
}

/* Find the rule and mapping requirements.
 * returns object offset if rule was found, zero if not found.
*/
ruletree_object_offset_t ruletree_get_mapping_requirements(
	ruletree_object_offset_t	rule_list_offs,
        const path_mapping_context_t *ctx,
        const struct path_entry_list *abs_virtual_source_path_list,
        int *min_path_lenp,
        int *call_translate_for_all_p,
	uint32_t fn_class)
{
	char    			*abs_virtual_source_path_string;
	ruletree_fsrule_t		*rule = NULL;
	ruletree_object_offset_t	rule_offs = 0;
	PROCESSCLOCK(clk1)

	START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "ruletree_get_mapping_requirements");
	abs_virtual_source_path_string = path_list_to_string(abs_virtual_source_path_list);

	if (rule_list_offs) {
		rule_offs = ruletree_find_rule(ctx,
			rule_list_offs, abs_virtual_source_path_string,
			strlen(abs_virtual_source_path_string),
			min_path_lenp, fn_class, &rule);
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: no rule list (,path=%s)",
			__func__, abs_virtual_source_path_string);
		rule_offs = 0;
		if (min_path_lenp) *min_path_lenp = 0;
	}

	if (call_translate_for_all_p) {
		if (rule) {
			switch (rule->rtree_fsr_action_type) {
			case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
			case SB2_RULETREE_FSRULE_ACTION_PROCFS:
				*call_translate_for_all_p = 1;
				break;
			default:
				*call_translate_for_all_p = 0;
				break;
			}
		} else {
			*call_translate_for_all_p = 0;
		}
	}

	free(abs_virtual_source_path_string);

	STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "");
	return (rule_offs);
}

/* returns an allocated buffer */
static char *ruletree_execute_replace_rule(
	const char *full_path,
	const char *replacement,
	ruletree_fsrule_t *rule)
{
	char		*new_path = NULL;
	uint32_t	selector_len;
	const char	*selector = offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs, &selector_len);

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_execute_replace_rule: orig='%s',replacement='%s',selector='%s'",
		full_path, replacement,selector);

	switch (rule->rtree_fsr_selector_type) {
	case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
	case SB2_RULETREE_FSRULE_SELECTOR_DIR:
		if (asprintf(&new_path, "%s%s",
		     replacement, full_path + selector_len) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR, "asprintf failed");
			return(NULL);
		}
		return(new_path);

	case SB2_RULETREE_FSRULE_SELECTOR_PATH:
		/* "path" may be shorter than prefix during path resolution */
		if (!strcmp(full_path, selector)) {
			return(strdup(replacement));
		}
		SB_LOG(SB_LOGLEVEL_DEBUG, "replacement failed");
		return(NULL);

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_execute_replace_rule: Unknown selector type %d",
			rule->rtree_fsr_selector_type);
	}

	return(NULL);
}

static int if_exists_then_map_to(ruletree_fsrule_t *action,
	const char *abs_clean_virtual_path, char **resultp)
{
	const char *map_to_target;
	char *test_path = NULL;

	*resultp = NULL;
	map_to_target = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);

	if (!strcmp(map_to_target, "/")) {
		test_path = strdup(abs_clean_virtual_path);
	} else {
		if (asprintf(&test_path, "%s%s", map_to_target, abs_clean_virtual_path) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR, "asprintf failed");
			return(0);
		}
	}
	if (sb_path_exists(test_path)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"if_exists_then_map_to: True '%s'", test_path);
		*resultp = test_path;
		return(1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"if_exists_then_map_to: False '%s'", test_path);
	free(test_path);
	return(0);
}

static int if_exists_then_replace_by(
	ruletree_fsrule_t *action, ruletree_fsrule_t *rule_selector,
	const char *abs_clean_virtual_path, char **resultp)
{
	char *test_path;
	const char *replacement = NULL;

	*resultp = NULL;
	replacement = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);

	test_path = ruletree_execute_replace_rule(abs_clean_virtual_path,
			replacement, rule_selector);
	if (!test_path) return(0);

	if (sb_path_exists(test_path)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"if_exists_then_replace_by: True '%s'", test_path);
		*resultp = test_path;
		return(1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"if_exists_then_replace_by: False '%s'", test_path);
	free(test_path);
	return(0);
}

static int if_exists_in(ruletree_fsrule_t *action,
                        const char *abs_clean_virtual_path)
{
	const char *map_to_target;
	char *test_path = NULL;

	map_to_target = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);

	if (!strcmp(map_to_target, "/")) {
		test_path = strdup(abs_clean_virtual_path);
	} else {
		if (asprintf(&test_path, "%s%s", map_to_target, abs_clean_virtual_path) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR, "asprintf failed");
			return(0);
		}
	}
	if (sb_path_exists(test_path)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"if_exists_in: True '%s' -> proceed to then_actions", test_path);
                free(test_path);
                return (1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"if_exists_in: False '%s'", test_path);
	free(test_path);
	return(0);
}

static char *execute_map_to(const char *abs_clean_virtual_path,
	const char *action_name, const char *prefix)
{
	char	*new_path = NULL;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %s", action_name,
		prefix ? prefix : "<NULL>");

	if (!prefix || !*prefix || !strcmp(prefix, "/")) {
		return(strdup(abs_clean_virtual_path));
	}
	if (asprintf(&new_path, "%s%s", prefix, abs_clean_virtual_path) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR, "asprintf failed");
		return(NULL);
	}
	return(new_path);
}

/* "standard actions" = use_orig_path, force_orig_path, map_to, replace_by */
static char *execute_std_action(
	ruletree_fsrule_t *rule_selector,
	ruletree_fsrule_t *action,
	const char *abs_clean_virtual_path, int *flagsp)
{
	const char	*cp;
	char	*procfs_result;
	char	*union_dir_result = NULL;
	char	*new_path = NULL;

	switch (action->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
		return(strdup(abs_clean_virtual_path));

	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
		/* flags should already contain SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH,
		 * but forcing it here won't hurt anyone. */
		*flagsp |= SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH;
		return(strdup(abs_clean_virtual_path));

	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT:
		/* flags should already contain it,
		 * but forcing it here won't hurt anyone. */
		*flagsp |= SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT;
		return(strdup(abs_clean_virtual_path));

	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);
		return(execute_map_to(abs_clean_virtual_path, "map_to", cp));

	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replace by '%s'", cp);
		new_path = ruletree_execute_replace_rule(abs_clean_virtual_path, cp/*replacement*/,
			rule_selector);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replaced: '%s'", cp);
		return(new_path);

	case SB2_RULETREE_FSRULE_ACTION_SET_PATH:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: set path to '%s'", cp);
		new_path = strdup(cp);
		return(new_path);

	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);
		cp = getenv(cp);
		return(execute_map_to(abs_clean_virtual_path, "map_to_value_of_env_var", cp));

	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs, NULL);
		cp = getenv(cp);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replace by env.var.value '%s'", cp);
		new_path = ruletree_execute_replace_rule(abs_clean_virtual_path, cp/*replacement*/,
			rule_selector);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replaced/2: '%s'", cp);
		return(new_path);

	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: /proc: %s", abs_clean_virtual_path);
		procfs_result = procfs_mapping_request(abs_clean_virtual_path);
		if (procfs_result) {
		        /* mapped to somewhere else */
			return(procfs_result);
		}
                /* no need to map this path */
		return(strdup(abs_clean_virtual_path));

	case SB2_RULETREE_FSRULE_ACTION_UNION_DIR:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: union_dir: %s", abs_clean_virtual_path);
		{
			ruletree_object_offset_t union_dir_list_offs = rule_selector->rtree_fsr_rule_list_link;
			int	num_real_dir_entries;
			const char	**src_paths = NULL;
			int	i;

			num_real_dir_entries = ruletree_objectlist_get_list_size(union_dir_list_offs);
			if (num_real_dir_entries > 0) {
				src_paths = calloc(num_real_dir_entries, sizeof(char*));
				for (i = 0; i < num_real_dir_entries; i++) {
					ruletree_object_offset_t str_offs;
					str_offs = ruletree_objectlist_get_item(union_dir_list_offs, i);
					src_paths[i] = offset_to_ruletree_string_ptr(str_offs, NULL);
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"execute_std_action: union_dir src_path[%d]: %s",
						i, src_paths[i]);
				}
				union_dir_result = prep_union_dir(abs_clean_virtual_path,
						src_paths, num_real_dir_entries);
			}
			if (src_paths) free(src_paths);
		}
		SB_LOG(SB_LOGLEVEL_DEBUG, "union_dir result = '%s'", union_dir_result);
		return(union_dir_result);

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Internal error: execute_std_action: action code is %d",
			action->rtree_fsr_action_type);
	}
	return(NULL);
}

static char *ruletree_execute_conditional_actions(
        const path_mapping_context_t *ctx,
        int result_log_level,
        const char *abs_clean_virtual_path,
        int *flagsp,
        const char **exec_policy_name_ptr,
	const char **errormsgp,
	ruletree_fsrule_t	*rule_selector)
{
	uint32_t	actions_list_size;
	uint32_t	i;
	ruletree_object_offset_t action_list_offs = rule_selector->rtree_fsr_rule_list_link;

	/* FIXME: these are not yet used. */
	(void)ctx;
	(void)result_log_level;
        (void)exec_policy_name_ptr;

	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_execute_conditional_actions for (%s)", abs_clean_virtual_path);
	actions_list_size = ruletree_objectlist_get_list_size(action_list_offs);
	if (actions_list_size == 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_execute_conditional_actions: action list not found or empty.");
		*errormsgp = "action list not found or empty.";
		return (NULL);
	}

	for (i = 0; i < actions_list_size; i++) {
		ruletree_fsrule_t	*action_cand_p;
		ruletree_object_offset_t action_offs;

		action_offs = ruletree_objectlist_get_item(action_list_offs, i);
		if (!action_offs) continue;

		/* "rule_selector" is the rule which matched, and
		 * brought us here (so it contains "dir","prefix"
		 * or "path").
		 * Each member in the "actions" array is a
                 * candidate for the rule which will be applied,
		 * i.e. a suitable member from that array gives
		 * instructions about what to do next */
		action_cand_p = offset_to_ruletree_fsrule_ptr(action_offs);
		if (action_cand_p) {
			char *mapping_result;

			if (action_cand_p->rtree_fsr_condition_type != 0) {
				const char *cond_str;
				const char *evp;

				cond_str = offset_to_ruletree_string_ptr(action_cand_p->rtree_fsr_condition_offs, NULL);
				SB_LOG(SB_LOGLEVEL_NOISE, "Condition test '%s'", cond_str);

				switch (action_cand_p->rtree_fsr_condition_type) {
				case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY:
					if (!cond_str) continue;	/* continue if no env.var.name */
					evp = getenv(cond_str);
					if (!evp || !*evp) continue; /* continue if empty */
					SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: env.var was not empty");
					break;	/* else test passed. */

				case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_EMPTY:
					if (!cond_str) continue;	/* continue if no env.var.name */
					evp = getenv(cond_str);
					if (evp && *evp) continue; /* continue if not empty */
					SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: env.var was empty");
					break;	/* else test passed. */

				case SB2_RULETREE_FSRULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS:
					if (!cond_str ||
					    !sbox_active_exec_policy_name ||
					    strcmp(cond_str, sbox_active_exec_policy_name)) {
						/* exec policy name did not match */
						continue;
					}
					SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: exec policy name matched");
					break;

				case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE:
					if (!cond_str) continue;	/* continue if no path */
					if (test_if_str_in_colon_separated_list_from_env(
						cond_str, "SBOX_REDIRECT_IGNORE")) {
						SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: redirect-ignore is active (%s)",
							cond_str);
					} else {
						SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: redirect-ignore is NOT active (%s)",
							cond_str);
						continue;
					}
					break;

				case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE:
					if (!cond_str) continue;	/* continue if no path */
					if (test_if_str_in_colon_separated_list_from_env(
						cond_str, "SBOX_REDIRECT_FORCE")) {
						SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: redirect-force is active (%s)",
							cond_str);
					} else {
						SB_LOG(SB_LOGLEVEL_NOISE, "Condition test: redirect-force is NOT active (%s)",
							cond_str);
						continue;
					}
					break;

                                case SB2_RULETREE_FSRULE_CONDITION_IF_EXISTS_IN:
                                  if (if_exists_in(action_cand_p, abs_clean_virtual_path)) {
                                    /* found, jump to the new rule tree branch */
                                    ruletree_object_offset_t then_actions_offset = action_cand_p->rtree_fsr_rule_list_link;
                                    if (!then_actions_offset) {
                                      SB_LOG(SB_LOGLEVEL_DEBUG, "if_exists_in: no then_actions found");
                                      /* continue with normal rule tree */
                                      continue;
                                    }
                                    else {
                                      SB_LOG(SB_LOGLEVEL_DEBUG, "if_exists_in: then_actions found (%d)", then_actions_offset);
                                      ruletree_fsrule_t new_rules = *rule_selector;
                                      new_rules.rtree_fsr_rule_list_link = then_actions_offset;
                                      return ruletree_execute_conditional_actions(ctx, result_log_level,
                                                                                  abs_clean_virtual_path, flagsp,
                                                                                  exec_policy_name_ptr, errormsgp,
                                                                                  &new_rules);
                                    }
                                  }
                                  /* not found, continue with normal rule tree */
                                  continue;
                                  break;

				default:
					SB_LOG(SB_LOGLEVEL_ERROR,
						"ruletree_execute_conditional_actions: "
						" unknown condition %d @%d",
						action_cand_p->rtree_fsr_condition_type, action_offs);
					goto unimplemented_action_error;
				}
			}

			switch (action_cand_p->rtree_fsr_action_type) {
			case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_MAP_TO:
				if (if_exists_then_map_to(action_cand_p,
				     abs_clean_virtual_path, &mapping_result)) {
					return(mapping_result);
				}
				break;

			case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_REPLACE_BY:
				if (if_exists_then_replace_by(action_cand_p,
				     rule_selector, abs_clean_virtual_path,
				     &mapping_result)) {
					return(mapping_result);
				}
				break;

			case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
			case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
			case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT:
			case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
			case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
			case SB2_RULETREE_FSRULE_ACTION_SET_PATH:
			case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
			case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
			case SB2_RULETREE_FSRULE_ACTION_PROCFS:
			case SB2_RULETREE_FSRULE_ACTION_UNION_DIR:
				return(execute_std_action(rule_selector, action_cand_p,
					abs_clean_virtual_path, flagsp));

			default:
				/* FIXME */
				goto unimplemented_action_error;
			}
		}
	}
	/* end of list is most probably a fatal error in the rule file. */
	SB_LOG(SB_LOGLEVEL_ERROR,
		"ruletree_execute_conditional_actions: End of conditional action list, "
		"probably caused by an error in the rule file.");
	/* FIXME. This should probably return the original path (compare with
	 * Lua code) */
	*errormsgp = "End of conditional action list";
	return (NULL);

    unimplemented_action_error:
	SB_LOG(SB_LOGLEVEL_ERROR,
		"Internal error: ruletree_execute_conditional_actions: Encountered "
		"an unknown conditional action.");
	*errormsgp = "unknown conditional action (internal error)";
	return (NULL);
}

char *ruletree_translate_path(
        const path_mapping_context_t *ctx,
        int result_log_level,
        const char *abs_clean_virtual_path,
        int *flagsp,
        const char **exec_policy_name_ptr,
	const char **errormsgp)
{
	char	*host_path = NULL;
	ruletree_fsrule_t *rule;
	PROCESSCLOCK(clk1)

	START_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, "ruletree_translate_path");
	*errormsgp = NULL;

	if (!ctx->pmc_ruletree_offset) {
		/* This might happen during initialization phase,
		 * when rule tree is not yet available */
		SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_translate_path: No rule tree");
		*errormsgp = "No rule tree";
		return(NULL);
	}
	rule = offset_to_ruletree_fsrule_ptr(ctx->pmc_ruletree_offset);

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_translate_path(%s)",
		abs_clean_virtual_path);

	/* execute rule */
	/* FIXME: should care about the R/O flag... */

	if (flagsp) *flagsp = rule->rtree_fsr_flags;
	if (exec_policy_name_ptr) {
		if (rule->rtree_fsr_exec_policy_name) {
			*exec_policy_name_ptr = offset_to_ruletree_string_ptr(rule->rtree_fsr_exec_policy_name, NULL);
		} else {
			*exec_policy_name_ptr = NULL;
		}
	}

	switch (rule->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE:
		SB_LOG(SB_LOGLEVEL_WARNING,
			"ruletree_translate_path: Forced fallback. This should never happen nowadays.");
		host_path = NULL;
		break;

	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
	case SB2_RULETREE_FSRULE_ACTION_SET_PATH:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
	case SB2_RULETREE_FSRULE_ACTION_UNION_DIR:
		host_path = execute_std_action(rule, rule, abs_clean_virtual_path, flagsp);
		break;

	case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
		host_path = ruletree_execute_conditional_actions(
			ctx, result_log_level, abs_clean_virtual_path,
			flagsp, exec_policy_name_ptr, errormsgp,
			rule);
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_translate_path: Unknown action code %d",
			rule->rtree_fsr_action_type);
		host_path = NULL;
		break;
	}
	if (host_path) {
		if (*host_path != '/') {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Mapping failed: Result is not absolute ('%s'->'%s')",
				abs_clean_virtual_path, host_path);
			host_path = NULL;
		} else {
			char *new_host_path = clean_and_log_fs_mapping_result(ctx,
				abs_clean_virtual_path, result_log_level, host_path, *flagsp);
			if (new_host_path == NULL) {
				SB_LOG(result_log_level,
					"Mapping failed ('%s')",
					abs_clean_virtual_path);
				*errormsgp = "Mapping failed";
			}
			free(host_path);
			host_path = new_host_path;
		}
	} else {
		SB_LOG(result_log_level,
			"Mapping failed: Fallback to Lua mapping ('%s')",
			abs_clean_virtual_path);
		*errormsgp = "No result from C mapping";
	}
	STOP_AND_REPORT_PROCESSCLOCK(SB_LOGLEVEL_INFO, &clk1, host_path);
	return(host_path);
}
