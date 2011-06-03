/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

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
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
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


/* Returns min.path length if a match is found, otherwise returns -1 */
static int ruletree_test_path_match(const char *full_path, ruletree_fsrule_t *rp)
{
	const char	*selector = NULL;
	const char	*match_type = "no match";
	int		result = -1;

	if (!rp || !full_path) {
		SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match fails"); 
		return(-1);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match (%s), type=%d", 
		full_path, rp->rtree_fsr_selector_type);

	selector = offset_to_ruletree_string_ptr(rp->rtree_fsr_selector_offs);

	switch (rp->rtree_fsr_selector_type) {
	case SB2_RULETREE_FSRULE_SELECTOR_PATH:
		if (selector) {
			if (!strcmp(full_path, selector)) {
				result = strlen(selector);
				match_type = "path";
			}
		}
		break;
	case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
		if (selector && (*selector != '\0')) {
			int	prefixlen = strlen(selector);

			if (!strncmp(full_path, selector, prefixlen)) {
				result = prefixlen;
				match_type = "prefix";
			}
		}
		break;
	case SB2_RULETREE_FSRULE_SELECTOR_DIR:
		if (selector && (*selector != '\0')) {
			int	prefixlen = strlen(selector);

			/* test a directory prefix: the next char after the
			 * prefix must be '\0' or '/', unless we are accessing
			 * the root directory */
			if (!strncmp(full_path, selector, prefixlen)) {
				if ( ((prefixlen == 1) && (*full_path=='/')) ||
				     (full_path[prefixlen] == '/') ||
				     (full_path[prefixlen] == '\0') ) {
					result = prefixlen;
					match_type = "dir";
				}
			}
		}
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_test_path_match: "
			"Unsupported selector type (rule='%s')",
			offset_to_ruletree_string_ptr(rp->rtree_fsr_name_offs));
		return(-1);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"ruletree_test_path_match '%s','%s' => %d (%s)",
		full_path, selector, result, match_type);
	return(result);
}

static ruletree_object_offset_t ruletree_find_rule(
        path_mapping_context_t *ctx,
	ruletree_object_offset_t rule_list_offs,
	const char *virtual_path,
	int *min_path_lenp,
	ruletree_fsrule_t	**rule_p)
{
	uint32_t	rule_list_size;
	uint32_t	i;

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

			min_path_len = ruletree_test_path_match(virtual_path, rp);

			if (min_path_len >= 0) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"ruletree_find_rule found rule @ %d",
					rule_offs);

				if (rp->rtree_fsr_func_class) {
					/* FIXME: Function classes are not yet supported.
					 * if we run into a rule which has func_class
					 * conditions, we can't do anything else than
					 * fallback to Lua mapping.
					*/
					return (0);
				}

				if (rp->rtree_fsr_binary_name) {
					const char	*bin_name_in_rule =
						offset_to_ruletree_string_ptr(rp->rtree_fsr_binary_name);
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
							virtual_path, min_path_lenp, rule_p);
						if (subtree_offs) return(subtree_offs);
					} else {
						SB_LOG(SB_LOGLEVEL_NOISE,
							"ruletree_find_rule: no link");
					}
					continue;
				}
				/* found it! */
				if (rule_p) *rule_p = rp;
				return(rule_offs);
			}
		}
	}
	return (0); /* failed to find it */
}

/* Find the rule and mapping requirements.
 * returns true if rule was found.
*/
int ruletree_get_mapping_requirements(
        path_mapping_context_t *ctx,
	int use_fwd_rules, /* flag */
        const struct path_entry_list *abs_virtual_source_path_list,
        int *min_path_lenp,
        int *call_translate_for_all_p)
{
	char    *abs_virtual_source_path_string;
	ruletree_object_offset_t rule_list_offs;
	const char *modename = sbox_session_mode;
	ruletree_fsrule_t	*rule = NULL;

	if (!modename) modename = "Default";
        abs_virtual_source_path_string = path_list_to_string(abs_virtual_source_path_list);

	if (use_fwd_rules) {
		rule_list_offs = find_from_mode_catalog(modename, "rules");
	} else {
		rule_list_offs = find_from_mode_catalog(modename, "rev_rules");
	}
	if (rule_list_offs) {
		ctx->pmc_ruletree_offset = ruletree_find_rule(ctx,
			rule_list_offs, abs_virtual_source_path_string,
			min_path_lenp, &rule);
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: no rule list (mode=%s,path=%s)",
			__func__, modename, abs_virtual_source_path_string);
		ctx->pmc_ruletree_offset = 0;
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

	/* return true if rule was found */
	return (ctx->pmc_ruletree_offset > 0); 
}

static char *ruletree_execute_replace_rule(
	const char *full_path,
	const char *replacement,
	ruletree_fsrule_t *rule)
{
	char	*new_path = NULL;
	const char	*selector = offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs);

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_execute_replace_rule: orig='%s',replacement='%s',selector='%s'",
		full_path, replacement,selector);

	switch (rule->rtree_fsr_selector_type) {
	case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
	case SB2_RULETREE_FSRULE_SELECTOR_DIR:
		if (asprintf(&new_path, "%s%s",
		     replacement, full_path + strlen(selector)) < 0) {
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
	char *map_to_target;
	char *test_path = NULL;

	*resultp = NULL;
	map_to_target = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);

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
	replacement = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);

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
	char	*new_path = NULL;

	switch (action->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
		return(strdup(abs_clean_virtual_path));

	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
		/* flags should already contain SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH,
		 * but forcing it there won't hurt anyone. */
		*flagsp |= SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH;
		return(strdup(abs_clean_virtual_path));
		break;

	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);
		return(execute_map_to(abs_clean_virtual_path, "map_to", cp));
		
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replace by '%s'", cp);
		new_path = ruletree_execute_replace_rule(abs_clean_virtual_path, cp/*replacement*/,
			rule_selector);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replaced: '%s'", cp);
		return(new_path);

	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);
		cp = getenv(cp);
		return(execute_map_to(abs_clean_virtual_path, "map_to_value_of_env_var", cp));

	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
		cp = offset_to_ruletree_string_ptr(action->rtree_fsr_action_offs);
		cp = getenv(cp);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replace by env.var.value '%s'", cp);
		new_path = ruletree_execute_replace_rule(abs_clean_virtual_path, cp/*replacement*/,
			rule_selector);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: replaced/2: '%s'", cp);
		break;

	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"execute_std_action: /proc: %s", abs_clean_virtual_path);
		cp = procfs_mapping_request(abs_clean_virtual_path);
		if (cp) {
		        /* mapped to somewhere else */
			return(cp);
		}
                /* no need to map this path */
		return(strdup(abs_clean_virtual_path));
		break;

	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Internal error: execute_std_action: action code is %d",
			action->rtree_fsr_action_type);
	}
	return(NULL);
}

char *ruletree_execute_conditional_actions(
        const path_mapping_context_t *ctx,
        int result_log_level,
        const char *abs_clean_virtual_path,
        int *flagsp,
        char **exec_policy_name_ptr,
	int *force_fallback_to_lua,
	ruletree_fsrule_t	*rule_selector)
{
	uint32_t	actions_list_size;
	uint32_t	i;
	ruletree_object_offset_t action_list_offs = rule_selector->rtree_fsr_rule_list_link;

	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_execute_conditional_actions for (%s)", abs_clean_virtual_path);
	actions_list_size = ruletree_objectlist_get_list_size(action_list_offs);
	if (actions_list_size == 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_execute_conditional_actions: action list not found or empty.");
		*force_fallback_to_lua = 1;
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

				cond_str = offset_to_ruletree_string_ptr(action_cand_p->rtree_fsr_condition_offs);
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

				default:
					SB_LOG(SB_LOGLEVEL_ERROR,
						"ruletree_execute_conditional_actions: "
						" unknown condition %d @%d",
						action_cand_p->rtree_fsr_condition_type, action_offs);
					goto unimplemented_action_fallback_to_lua;
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
			case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
			case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
			case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
			case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
			case SB2_RULETREE_FSRULE_ACTION_PROCFS:
				return(execute_std_action(rule_selector, action_cand_p,
					abs_clean_virtual_path, flagsp));

			/* FIXME FIXME Implement other action types, too. */

			default:
				/* FIXME */
				goto unimplemented_action_fallback_to_lua;
			}
		}
	}
	/* FIXME: end of list is more probably a fatal error than
	 * something that should cause a fallback, fix that later */
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"ruletree_translate_path: End of conditional action list.");
	*force_fallback_to_lua = 1;
	return (NULL);
			
    unimplemented_action_fallback_to_lua:
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"ruletree_translate_path: Encountered a conditional action which has not been implemented yet.");
	*force_fallback_to_lua = 1;
	return (NULL);
}

char *ruletree_translate_path(
        const path_mapping_context_t *ctx,
        int result_log_level,
        const char *abs_clean_virtual_path,
        int *flagsp,
        char **exec_policy_name_ptr,
	int *force_fallback_to_lua)
{
	const char	*cp;
	char	*host_path = NULL;
	ruletree_fsrule_t *rule;

	*force_fallback_to_lua = 0;

	if (!ctx->pmc_ruletree_offset) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_translate_path: No rule tree");
		*force_fallback_to_lua = 1;
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
			*exec_policy_name_ptr = offset_to_ruletree_string_ptr(rule->rtree_fsr_exec_policy_name);
		} else {
			*exec_policy_name_ptr = NULL;
		}
	}

	switch (rule->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_translate_path: Forced fallback.");
		host_path = NULL;
		break;

	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
		host_path = execute_std_action(rule, rule, abs_clean_virtual_path, flagsp);
		break;

	case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
		host_path = ruletree_execute_conditional_actions(
			ctx, result_log_level, abs_clean_virtual_path,
			flagsp, exec_policy_name_ptr, force_fallback_to_lua,
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
			free(host_path);
			host_path = new_host_path;
		}
	} else {
		SB_LOG(result_log_level,
			"Mapping failed: Fallback to Lua mapping ('%s')",
			abs_clean_virtual_path);
		*force_fallback_to_lua = 1;
	}
	return(host_path);
}

