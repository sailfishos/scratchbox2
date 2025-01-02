/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Exec Policy Selection.
 * ------------------------------------
 *
 * The exec policy selection table can contain three types of rules:
 *      { prefix = "/path/prefix", exec_policy_name = "policyname" }
 *      { path = "/exact/path/to/program", exec_policy_name = "policyname" }
 *      { dir = "/directory/path", exec_policy_name = "policyname" }
*/

#include <config.h>

#include <sys/mman.h>

#include <mapping.h>
#include <sb2.h>

#include "libsb2.h"
#include "exported.h"
#include "sb2_execs.h"



/* FIXME: This is currently a slightly modified copy of ruletree_test_path_match()
 *    
 * Returns min.path length if a match is found, otherwise returns -1 */
static int test_path_match(const char *full_path, size_t full_path_len,
	uint32_t selector_type, ruletree_object_offset_t selector_offs)
{
	const char	*selector = NULL;
	const char	*match_type = "no match";
	int		result = -1;
	uint32_t	selector_len;

	if (!selector_type || !full_path) {
		SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match fails"); 
		return(-1);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_test_path_match (%s), type=%d", 
		full_path, selector_type);

	selector = offset_to_ruletree_string_ptr(selector_offs,
		&selector_len);

	switch (selector_type) {
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
			"%s: Unsupported selector type", __func__);
		return(-1);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"%s: '%s' (%u), '%s' (%u) => %d (%s)",
		__func__, full_path, full_path_len, selector, selector_len, result, match_type);
	return(result);
}

const char *find_exec_policy_name(const char *mapped_path, const char *virtual_path)
{
	static ruletree_object_offset_t		policy_selection_rules_offs = 0;
	uint32_t	list_size;
	unsigned int	i;
	int		mapped_path_len;
	static const char	*modename = NULL;

	(void)virtual_path; /* not used */

	if (!policy_selection_rules_offs) {
		modename = sbox_session_mode;
		if (!modename)
			modename = ruletree_catalog_get_string("MODES", "#default");
		if (!modename) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: modename not found", __func__);
			return(NULL);
		}

		policy_selection_rules_offs = ruletree_catalog_get(
			"exec_policy_selection", modename);

		if (!policy_selection_rules_offs) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: no exec policy selection rules for mode '%s'",
				__func__, modename);
			return(NULL);
		}
	}
	list_size = ruletree_objectlist_get_list_size(policy_selection_rules_offs);
	mapped_path_len = strlen(mapped_path);

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: path='%s'", __func__, mapped_path);
	for (i = 0; i < list_size; i++) {
		ruletree_object_offset_t	rule_offs;

		rule_offs = ruletree_objectlist_get_item(policy_selection_rules_offs, i);
		if (rule_offs) {
			ruletree_exec_policy_selection_rule_t   *rule;

			rule = offset_to_ruletree_object_ptr(
				rule_offs, SB2_RULETREE_OBJECT_TYPE_EXEC_SEL_RULE);
			if (rule) {
				if (test_path_match(mapped_path, mapped_path_len,
					rule->rtree_xps_type, rule->rtree_xps_selector_offs) >= 0) {
					const char *epn = offset_to_ruletree_string_ptr(
						rule->rtree_xps_exec_policy_name_offs, NULL);
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"%s: exec policy found, #%u '%s'",
						__func__, i, epn);
					return(epn);
				}
			}
		}
	}
	SB_LOG(SB_LOGLEVEL_ERROR,
		"%s: exec policy was not found (mode='%s'), default rule is missing?",
		__func__, modename);
	return(NULL);
}

