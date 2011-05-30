/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* dump contents from the memory-mapped rule tree.
 * this is a debugging tool for developers.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define lua_State void /* FIXME */

#include "rule_tree.h"

/* Fake logger. needed by the ruletree routines */

int sb_loglevel__ = 2;

extern void sblog_printf_line_to_logfile(const char *file, int line,
        int level, const char *format,...);

void sblog_printf_line_to_logfile(const char *file, int line,
        int level, const char *format,...)
{
	va_list ap;

	(void)level;
	va_start(ap, format);
        vfprintf(stderr, format, ap);
        fprintf(stderr, "[%s:%d]\n", file, line);
        va_end(ap);
}

extern int open_nomap_nolog(const char *pathname, int flags, mode_t mode);

int open_nomap_nolog(const char *pathname, int flags, mode_t mode)
{
	return open(pathname, flags, mode);
}

char *sbox_session_dir = NULL; /* Fake var, referenced by the library=>must have something*/ 

/* -------------------- */

static void dump_catalog(ruletree_object_offset_t catalog_offs, const char *catalog_name, int indent);
static void dump_objectlist(ruletree_object_offset_t list_offs, int indent);

static char *rule_dumped = NULL;

static void print_indent(int i)
{
	while(i--) putchar('\t');
}

static void dump_rules(ruletree_object_offset_t offs, int indent)
{
	ruletree_fsrule_t	*rule = offset_to_ruletree_fsrule_ptr(offs);
	const char *rule_list_link_label = "??";

	if (rule_dumped[offs]) {
		print_indent(indent + 1);
		printf("[ => @ %u]\n", offs);
		return;
	}
	rule_dumped[offs] = 1;

	print_indent(indent);
	printf("{ Rule[%u]:\n", (unsigned)offs);

	if (rule->rtree_fsr_name_offs) {
		print_indent(indent+1);
		printf("name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_name_offs));
	}

	if (rule->rtree_fsr_selector_type) {
		print_indent(indent+1);
		printf("IF: ");
		switch (rule->rtree_fsr_selector_type) {
		case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
			printf("prefix '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs));
			break;
		case SB2_RULETREE_FSRULE_SELECTOR_DIR:
			printf("dir '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs));
			break;
		case SB2_RULETREE_FSRULE_SELECTOR_PATH:
			printf("path '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs));
			break;
		default:
			printf("ERROR: Unknown selector type %d\n",
				rule->rtree_fsr_selector_type);
			break;
		}
	}

	if (rule->rtree_fsr_condition_type) {
		print_indent(indent+1);
		printf("CONDITINAL: ");
		switch (rule->rtree_fsr_condition_type) {
		case SB2_RULETREE_FSRULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS:
			printf("if_active_exec_policy_is '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_condition_offs));
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE:
			printf("if_redirect_ignore_is_active '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_condition_offs));
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE:
			printf("if_redirect_force_is_active '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_condition_offs));
			break;
		default:
			printf("ERROR: Unknown selector type %d\n",
				rule->rtree_fsr_selector_type);
			break;
		}
	}

	print_indent(indent+1);
	printf("ACTION: ");
	switch (rule->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE:
		printf("FALLBACK_TO_OLD_MAPPING_ENGINE\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
		printf("use_orig_path\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
		printf("force_orig_path\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
		printf("replace_by '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs));
		break;
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
		printf("map_to '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs));
		break;
	case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
		printf("actions => %d\n",
			rule->rtree_fsr_rule_list_link);
		rule_list_link_label = "actions";
		break;
	case SB2_RULETREE_FSRULE_ACTION_SUBTREE:
		printf("subtree => %d\n",
			rule->rtree_fsr_rule_list_link);
		rule_list_link_label = "rules";
		break;
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_MAP_TO:
		printf("if_exists_then_map_to '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs));
		break;
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_REPLACE_BY:
		printf("if_exists_then_replace_by '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs));
		break;
	default:
		printf("ERROR: Unknown action type %d\n",
			rule->rtree_fsr_action_type);
		break;
	}

	if (rule->rtree_fsr_rule_list_link) {
		print_indent(indent+1);
		printf("%s = {\n", rule_list_link_label);
		dump_objectlist(rule->rtree_fsr_rule_list_link, indent + 2);
		print_indent(indent+1);
		printf("}\n");
	}
	print_indent(indent);
	printf("}\n");
}

static void dump_objectlist(ruletree_object_offset_t list_offs, int indent)
{
	uint32_t	list_size = ruletree_objectlist_get_list_size(list_offs);
	uint32_t	i;

	print_indent(indent);
	printf("{ list[%u], size=%u:\n", (unsigned)list_offs, list_size);

	for (i = 0; i < list_size; i++) {
		ruletree_object_offset_t	item_offs;

		item_offs = ruletree_objectlist_get_item(list_offs, i);

		print_indent(indent);
		printf("#%d:\n", i);

		if (item_offs) {
			ruletree_object_hdr_t *hdr = offset_to_ruletree_object_ptr(
				item_offs, 0/*any type is ok*/);
			if (hdr) {
				switch (hdr->rtree_obj_type) {
				case SB2_RULETREE_OBJECT_TYPE_OBJECTLIST:
					print_indent(indent+1);
					printf("List:\n");
					dump_objectlist(item_offs, indent+2);
					break;
				case SB2_RULETREE_OBJECT_TYPE_FSRULE:
					print_indent(indent+1);
					printf("FS rule:\n");
					dump_rules(item_offs, indent+2);
					break;
				default:
					print_indent(indent+1);
					printf("Unsupported type\n");
					break;
				}
			}
		}
	}
	print_indent(indent);
	printf("}\n");
}

static void dump_catalog(ruletree_object_offset_t catalog_offs, const char *catalog_name, int indent)
{
	ruletree_catalog_entry_t	*catalog;
	ruletree_catalog_entry_t	*catp;

	if (!catalog_offs) {
		ruletree_hdr_t	*treehdr = (ruletree_hdr_t*)offset_to_ruletree_object_ptr(0,
					SB2_RULETREE_OBJECT_TYPE_FILEHDR);
		catalog_offs = treehdr->rtree_hdr_root_catalog;
	}

	if (rule_dumped[catalog_offs]) {
		print_indent(indent);
		printf("[ => Catalog @ %u '%s']\n", catalog_offs, catalog_name);
		return;
	}
	rule_dumped[catalog_offs] = 1;

	print_indent(indent);
	printf("Catalog @ %u '%s':\n", catalog_offs, catalog_name);

	catalog = offset_to_ruletree_object_ptr(catalog_offs,
		SB2_RULETREE_OBJECT_TYPE_CATALOG);
	
	if (!catalog) {
		print_indent(indent+1);
		printf("[empty]\n");
		return;
	}

	/* first, print contents of the catalog itself */
	for (catp = catalog; catp;
	     catp = catp->rtree_cat_next_entry_offs ?
		offset_to_ruletree_object_ptr(catp->rtree_cat_next_entry_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG) : NULL) {

		const char *name = "??";

		print_indent(indent+1);
		if (catp->rtree_cat_name_offs) {
			name = offset_to_ruletree_string_ptr(catp->rtree_cat_name_offs);
			if (name) {
				printf("'%s'\t", name);
			} else {
				printf("<invalid name offset>\t");
			}
		} else {
			printf("<no name>\t");
		}

		if (catp->rtree_cat_value_offs) {
			ruletree_object_hdr_t *hdr = offset_to_ruletree_object_ptr(
				catp->rtree_cat_value_offs, 0/*any type is ok*/);

			if (hdr) {
				switch (hdr->rtree_obj_type) {
				case SB2_RULETREE_OBJECT_TYPE_FILEHDR:
					printf("FILEHDR\t");
					break;
				case SB2_RULETREE_OBJECT_TYPE_CATALOG:
					printf("CATALOG: -> %u",
						catp->rtree_cat_value_offs);
					break;
				case SB2_RULETREE_OBJECT_TYPE_FSRULE:
					printf("RULE: -> %u",
						catp->rtree_cat_value_offs);
					break;
				case SB2_RULETREE_OBJECT_TYPE_STRING:
					printf("STRING\t");
					break;
				case SB2_RULETREE_OBJECT_TYPE_OBJECTLIST:
					printf("LIST: -> %u",
						catp->rtree_cat_value_offs);
					break;
				default:
					printf("<unknown type>\t");
					break;
				}
			} else {
				printf("<invalid value offset>\t");
			}
		} else {
			printf("<no value>\t");
		}
		printf("\n");
	}

	/* next, process again all entries that need recursive processing  */
	for (catp = catalog; catp;
	     catp = catp->rtree_cat_next_entry_offs ?
		offset_to_ruletree_object_ptr(catp->rtree_cat_next_entry_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG) : NULL) {

		const char *name = "??";

		if (catp->rtree_cat_name_offs) {
			name = offset_to_ruletree_string_ptr(catp->rtree_cat_name_offs);
		}

		if (catp->rtree_cat_value_offs) {
			ruletree_object_hdr_t *hdr = offset_to_ruletree_object_ptr(
				catp->rtree_cat_value_offs, 0/*any type is ok*/);

			if (hdr) {
				switch (hdr->rtree_obj_type) {
				case SB2_RULETREE_OBJECT_TYPE_CATALOG:
					print_indent(indent+1);
					printf("Catalog @ %d\n", catp->rtree_cat_value_offs);
					dump_catalog(catp->rtree_cat_value_offs, name, indent+2);
					printf("\n");
					break;
				case SB2_RULETREE_OBJECT_TYPE_OBJECTLIST:
					printf("\n");
					dump_objectlist(catp->rtree_cat_value_offs, indent+2);
					break;
				default:
					/* ignore it. */
					break;
				}
			}
		}
	}

	print_indent(indent);
	printf("End of Catalog @ %u '%s'.\n", catalog_offs, catalog_name);
}

int main(int argc, char *argv[])
{
	char    *session_dir = NULL;
	char	*rule_tree_path = NULL;
	int	opt;

	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			sb_loglevel__ = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Illegal option\n");
			exit(1);
		}
	}

	session_dir = getenv("SBOX_SESSION_DIR");
	if (!session_dir) {
		fprintf(stderr, "ERROR: no session "
			"(SBOX_SESSION_DIR is not set) - this program"
			" must be used inside a Scratchbox 2 session\n");
		exit(1);
	}

	if (asprintf(&rule_tree_path, "%s/RuleTree.bin", session_dir) < 0) {
		exit(1);
	}


	printf("Attach tree (%s)\n", rule_tree_path);
	if (attach_ruletree(rule_tree_path, 0, 0) < 0) {
		printf("Attach failed!\n");
	} else {
		size_t siz;
		printf("Attach OK!\n");
		siz = ruletree_get_file_size();
		rule_dumped = calloc(siz, sizeof(*rule_dumped));

		dump_catalog(0, "", 0);
	}
	return(0);
}

