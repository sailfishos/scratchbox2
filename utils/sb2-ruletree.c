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
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define lua_State void /* FIXME */

#include "rule_tree.h"
#include "mapping.h"

static int print_ruletree_offsets = 0;	/* can be set with -o */

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

static void print_ruletree_object_type(ruletree_object_offset_t obj_offs);
static void print_ruletree_object_recurse(int indent, const char *name, ruletree_object_offset_t obj_offs);

static char *rule_dumped = NULL;

static void print_indent(int i)
{
	while(i--) putchar('\t');
}

static void dump_rules(ruletree_object_offset_t offs, int indent)
{
	ruletree_fsrule_t	*rule = offset_to_ruletree_fsrule_ptr(offs);
	const char *rule_list_link_label = "??";

	if (print_ruletree_offsets) {
		if (rule_dumped[offs]) {
			print_indent(indent + 1);
			printf("[ => @ %u]\n", offs);
			return;
		}
	}
	rule_dumped[offs] = 1;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("{ Rule[%u]:\n", (unsigned)offs);
	} else {
		printf("{ Rule:\n");
	}

	if (rule->rtree_fsr_name_offs) {
		print_indent(indent+1);
		printf("name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_name_offs, NULL));
	}

	if (rule->rtree_fsr_selector_type) {
		print_indent(indent+1);
		printf("IF: ");
		switch (rule->rtree_fsr_selector_type) {
		case SB2_RULETREE_FSRULE_SELECTOR_PREFIX:
			printf("prefix '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs, NULL));
			break;
		case SB2_RULETREE_FSRULE_SELECTOR_DIR:
			printf("dir '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs, NULL));
			break;
		case SB2_RULETREE_FSRULE_SELECTOR_PATH:
			printf("path '%s'\n",
				offset_to_ruletree_string_ptr(rule->rtree_fsr_selector_offs, NULL));
			break;
		default:
			printf("ERROR: Unknown selector type %d\n",
				rule->rtree_fsr_selector_type);
			break;
		}
	}

	if (rule->rtree_fsr_condition_type) {
		const char *condstr = offset_to_ruletree_string_ptr(rule->rtree_fsr_condition_offs, NULL);

		print_indent(indent+1);
		printf("CONDITIONAL: ");
		switch (rule->rtree_fsr_condition_type) {
		case SB2_RULETREE_FSRULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS:
			printf("if_active_exec_policy_is '%s'\n", condstr);
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE:
			printf("if_redirect_ignore_is_active '%s'\n", condstr);
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE:
			printf("if_redirect_force_is_active '%s'\n", condstr);
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY:
			printf("if_env_var_is_not_empty '%s'\n", condstr);
			break;
		case SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_EMPTY:
			printf("if_env_var_is_empty '%s'\n", condstr);
			break;
		default:
			printf("ERROR: Unknown condition type %d\n",
				rule->rtree_fsr_condition_type);
			break;
		}
	}

	if (rule->rtree_fsr_func_class) {
		print_indent(indent+1);
		printf("IF_CLASS: 0x%X ( ", rule->rtree_fsr_func_class);
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_OPEN)
		       printf("open ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_STAT)
		       printf("stat ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_EXEC)
		       printf("exec ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_SOCKADDR)
		       printf("sockaddr ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_FTSOPEN)
		       printf("ftsopen ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_GLOB)
		       printf("glob ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_GETCWD)
		       printf("getcwd ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_REALPATH)
		       printf("realpath ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_L10N)
		       printf("l10n ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_MKNOD)
		       printf("mknod ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_RENAME)
		       printf("rename ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_SYMLINK)
		       printf("symlink ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_CREAT)
		       printf("creat ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_PROC_FS_OP)
		       printf("proc_fs_op ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_SET_TIMES)
		       printf("set_times ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_REMOVE)
		       printf("remove ");
		if (rule->rtree_fsr_func_class & SB2_INTERFACE_CLASS_CHROOT)
		       printf("chroot ");
		printf(")\n");
	}
	
	if (rule->rtree_fsr_binary_name) {
		const char *bin_name = offset_to_ruletree_string_ptr(rule->rtree_fsr_binary_name, NULL);
		print_indent(indent+1);
		printf("BINARY_NAME: '%s'\n", bin_name);
	}

	if (rule->rtree_fsr_exec_policy_name) {
		const char *ep_name = offset_to_ruletree_string_ptr(rule->rtree_fsr_exec_policy_name, NULL);
		print_indent(indent+1);
		printf("EXEC_POLICY_NAME: '%s'\n", ep_name);
	}

	print_indent(indent+1);
	printf("ACTION: ");
	switch (rule->rtree_fsr_action_type) {
	case SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE:
		printf("FALLBACK_TO_OLD_MAPPING_ENGINE\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_PROCFS:
		printf("sb2_procfs_mapper\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_UNION_DIR:
#if 0
		printf("union_dir => rule->rtree_fsr_rule_list_link\n");
#endif
		printf("\n");
		rule_list_link_label = "union_dir";
		break;	
	case SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH:
		printf("use_orig_path\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH:
		printf("force_orig_path\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH_UNLESS_CHROOT:
		printf("force_orig_path_unless_chroot\n");
		break;	
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY:
		printf("replace_by '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_SET_PATH:
		printf("set_path '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO:
		printf("map_to '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR:
		printf("replace_by_value_of_env_var '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR:
		printf("map_to_value_of_env_var '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS:
		if (print_ruletree_offsets) {
			printf("actions => %d\n",
				rule->rtree_fsr_rule_list_link);
		} else {
			printf("\n");
		}
		rule_list_link_label = "actions";
		break;
	case SB2_RULETREE_FSRULE_ACTION_SUBTREE:
		if (print_ruletree_offsets) {
			printf("subtree => %d\n",
				rule->rtree_fsr_rule_list_link);
		} else {
			printf("\n");
		}
		rule_list_link_label = "rules";
		break;
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_MAP_TO:
		printf("if_exists_then_map_to '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
		break;
	case SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_REPLACE_BY:
		printf("if_exists_then_replace_by '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_fsr_action_offs, NULL));
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

static void dump_exec_selection_rules(ruletree_object_offset_t offs, int indent)
{
	ruletree_exec_policy_selection_rule_t	*rule = offset_to_ruletree_object_ptr(
		offs, SB2_RULETREE_OBJECT_TYPE_EXEC_SEL_RULE);

	if (print_ruletree_offsets) {
		if (rule_dumped[offs]) {
			print_indent(indent + 1);
			printf("[ => @ %u]\n", offs);
			return;
		}
	}
	rule_dumped[offs] = 1;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("{ Exec policy selection rule[%u]:\n", (unsigned)offs);
	} else {
		printf("{ Exec policy selection rule:\n");
	}
	print_indent(indent+1);
	printf("type = 0x%X\n", rule->rtree_xps_type);
	if (rule->rtree_xps_selector_offs) {
		print_indent(indent+1);
		printf("selector = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_xps_selector_offs, NULL));
	}
	if (rule->rtree_xps_exec_policy_name_offs) {
		print_indent(indent+1);
		printf("exec_policy_name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_xps_exec_policy_name_offs, NULL));
	}
	print_indent(indent+1);
	printf("flags = 0x%X\n", rule->rtree_xps_flags);
	print_indent(indent);
	printf("}\n");
}

static void dump_exec_pp_rules(ruletree_object_offset_t offs, int indent)
{
	ruletree_exec_preprocessing_rule_t	*rule = offset_to_exec_preprocessing_rule_ptr(offs);

	if (print_ruletree_offsets) {
		if (rule_dumped[offs]) {
			print_indent(indent + 1);
			printf("[ => @ %u]\n", offs);
			return;
		}
	}
	rule_dumped[offs] = 1;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("{ Exec preprocessing rule[%u]:\n", (unsigned)offs);
	} else {
		printf("{ Exec preprocessing rule:\n");
	}

	if (rule->rtree_xpr_binary_name_offs) {
		print_indent(indent+1);
		printf("binary_name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_xpr_binary_name_offs, NULL));
	}

#define EXECPP_PRINT_LIST(name, field) \
	if (rule->field) { \
		print_indent(indent+1); \
		printf(name " = {\n"); \
		dump_objectlist(rule->field, indent + 2); \
		print_indent(indent+1); \
		printf("}\n"); \
	}

	EXECPP_PRINT_LIST("path_prefixes", rtree_xpr_path_prefixes_table_offs)
	EXECPP_PRINT_LIST("add_head", rtree_xpr_add_head_table_offs)
	EXECPP_PRINT_LIST("add_options", rtree_xpr_add_options_table_offs)
	EXECPP_PRINT_LIST("add_tail", rtree_xpr_add_tail_table_offs)
	EXECPP_PRINT_LIST("remove", rtree_xpr_remove_table_offs)

	if (rule->rtree_xpr_new_filename_offs) {
		print_indent(indent+1);
		printf("new_filename = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_xpr_new_filename_offs, NULL));
	}
	if (rule->rtree_xpr_disable_mapping) {
		print_indent(indent+1);
		printf("disable_mapping = true\n");
	}
	print_indent(indent);
	printf("}\n");
}

static void dump_net_rules(ruletree_object_offset_t offs, int indent)
{
	ruletree_net_rule_t	*rule = offset_to_ruletree_object_ptr(offs,
		SB2_RULETREE_OBJECT_TYPE_NET_RULE);

	if (print_ruletree_offsets) {
		if (rule_dumped[offs]) {
			print_indent(indent + 1);
			printf("[ => @ %u]\n", offs);
			return;
		}
	}
	rule_dumped[offs] = 1;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("{ net rule[%u]:\n", (unsigned)offs);
	} else {
		printf("{ net rule:\n");
	}
	print_indent(indent + 1);
	switch(rule->rtree_net_ruletype) {
	case SB2_RULETREE_NET_RULETYPE_DENY:
		printf("ruletype = deny\n");
		break;
	case SB2_RULETREE_NET_RULETYPE_ALLOW:
		printf("ruletype = allow\n");
		break;
	case SB2_RULETREE_NET_RULETYPE_RULES:
		printf("ruletype = rules\n");
		break;
	default:
		printf("ruletype = %d (UNKNOWN)\n", rule->rtree_net_ruletype);
		break;
	}

	if (rule->rtree_net_func_name) {
		print_indent(indent + 1);
		printf("func_name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_net_func_name, NULL));
	}
	if (rule->rtree_net_binary_name) {
		print_indent(indent + 1);
		printf("binary_name = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_net_binary_name, NULL));
	}
	if (rule->rtree_net_address) {
		print_indent(indent + 1);
		printf("address = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_net_address, NULL));
	}
	if (rule->rtree_net_port) {
		print_indent(indent + 1);
		printf("port = %d\n", rule->rtree_net_port);
	}
	if (rule->rtree_net_new_address) {
		print_indent(indent + 1);
		printf("new_address = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_net_new_address, NULL));
	}
	if (rule->rtree_net_new_port) {
		print_indent(indent + 1);
		printf("new_port = %d\n", rule->rtree_net_new_port);
	}
	if (rule->rtree_net_log_msg) {
		print_indent(indent + 1);
		if (rule->rtree_net_log_level) {
			printf("log_level = %d, ", rule->rtree_net_log_level);
		}
		printf("log_msg = '%s'\n",
			offset_to_ruletree_string_ptr(rule->rtree_net_log_msg, NULL));
	}
	if (rule->rtree_net_errno) {
		print_indent(indent + 1);
		printf("errno = %d '%s'\n",
			rule->rtree_net_errno, strerror(rule->rtree_net_errno));
	}
	if (rule->rtree_net_rules) {
		dump_objectlist(rule->rtree_net_rules, indent+1);
	}
	print_indent(indent);
	printf("}\n");
}

static void dump_objectlist(ruletree_object_offset_t list_offs, int indent)
{
	uint32_t	list_size = ruletree_objectlist_get_list_size(list_offs);
	uint32_t	i;
	const char	*cp;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("{ list[%u], size=%u:\n", (unsigned)list_offs, list_size);
	} else {
		printf("{ list, size=%u:\n", list_size);
	}

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
				case SB2_RULETREE_OBJECT_TYPE_EXEC_PP_RULE:
					dump_exec_pp_rules(item_offs, indent+1);
					break;
				case SB2_RULETREE_OBJECT_TYPE_EXEC_SEL_RULE:
					dump_exec_selection_rules(item_offs, indent+1);
					break;
				case SB2_RULETREE_OBJECT_TYPE_NET_RULE:
					dump_net_rules(item_offs, indent+1);
					break;
				case SB2_RULETREE_OBJECT_TYPE_STRING:
					print_indent(indent+1);
					printf("STRING ");
					cp = offset_to_ruletree_string_ptr(item_offs, NULL);
					if (cp) printf("'%s'\n", cp);
					else printf("NULL\n");
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

static void dump_bintree(ruletree_object_offset_t tree_offs, int indent,
	int depth, int *maxdepth, int *nodes)
{
	ruletree_object_hdr_t *hdr;
	int	maxd = 0, n = 0;

	hdr = offset_to_ruletree_object_ptr(
		tree_offs, SB2_RULETREE_OBJECT_TYPE_BINTREE);

	if (!maxdepth) maxdepth = &maxd;
	if (!nodes) nodes = &n;
	if (*maxdepth < depth) *maxdepth = depth;
	(*nodes)++;

	print_indent(indent);
	if (!hdr) {
		printf("{ INVALID, not a bintree node [%u]}\n", (unsigned)tree_offs);
	} else {
		ruletree_bintree_t *bthdr = (ruletree_bintree_t*)hdr;

		printf("{ bintree");
		if (print_ruletree_offsets) {
			printf("[%u]", (unsigned)tree_offs);
		}
		printf(", key=(%llu,%llu) less=%u, more=%u, value @%u\n",
			(long long unsigned int)bthdr->rtree_bt_key1,
			(long long unsigned int)bthdr->rtree_bt_key2,
			bthdr->rtree_bt_link_less, bthdr->rtree_bt_link_more,
			bthdr->rtree_bt_value);
		if (bthdr->rtree_bt_value) {
			print_indent(indent+1);
			print_ruletree_object_type(bthdr->rtree_bt_value);
			printf("\n");
			print_ruletree_object_recurse(indent+1, NULL, bthdr->rtree_bt_value);
		}

		if (bthdr->rtree_bt_link_less) {
			print_indent(indent);
			printf("  Less:\n");
			dump_bintree(bthdr->rtree_bt_link_less, indent+1, depth+1, maxdepth, nodes);
		}
		if (bthdr->rtree_bt_link_more) {
			print_indent(indent);
			printf("  More:\n");
			dump_bintree(bthdr->rtree_bt_link_more, indent+1, depth+1, maxdepth, nodes);
		}
	}
	if (depth <= 1) {
		print_indent(indent);
		printf("  Bintree maddepth = %d, nodes = %d\n", *maxdepth, *nodes);
	}
	print_indent(indent);
	printf("}\n");
}

static void print_ruletree_object_type(ruletree_object_offset_t obj_offs)
{
	ruletree_object_hdr_t *hdr;

	hdr = offset_to_ruletree_object_ptr(obj_offs, 0/*any type is ok*/);

	if (hdr) {
		const char *cp;
		uint32_t *uip;

		if (print_ruletree_offsets) {
			printf("@%u: ", obj_offs);
		}
		switch (hdr->rtree_obj_type) {
		case SB2_RULETREE_OBJECT_TYPE_FILEHDR:
			printf("FILEHDR");
			break;
		case SB2_RULETREE_OBJECT_TYPE_CATALOG:
			printf("CATALOG");
			break;
		case SB2_RULETREE_OBJECT_TYPE_FSRULE:
			printf("FSRULE");
			break;
		case SB2_RULETREE_OBJECT_TYPE_STRING:
			printf("STRING\t");
			cp = offset_to_ruletree_string_ptr(obj_offs, NULL);
			if (cp) printf("'%s'", cp);
			else printf("NULL");
			break;
		case SB2_RULETREE_OBJECT_TYPE_OBJECTLIST:
			printf("LIST");
			break;
		case SB2_RULETREE_OBJECT_TYPE_BINTREE:
			printf("BINTREE");
			break;
		case SB2_RULETREE_OBJECT_TYPE_INODESTAT:
			{
				ruletree_inodestat_t *fsp;

				fsp = (ruletree_inodestat_t*)hdr;
				if (fsp) {
					int	a = fsp->rtree_inode_simu.inodesimu_active_fields;

					printf("INODESTAT: dev=%lld ino=%lld act=0x%X",
						(long long)fsp->rtree_inode_simu.inodesimu_dev,
						(long long)fsp->rtree_inode_simu.inodesimu_ino, a);
					if (a & RULETREE_INODESTAT_SIM_UID) 
						printf(" uid=%d",
							(int)fsp->rtree_inode_simu.inodesimu_uid);
					if (a & RULETREE_INODESTAT_SIM_GID) 
						printf(" gid=%d",
							(int)fsp->rtree_inode_simu.inodesimu_gid);
					if (a & RULETREE_INODESTAT_SIM_MODE) 
						printf(" mode=0%o",
							(int)fsp->rtree_inode_simu.inodesimu_mode);
					if (a & RULETREE_INODESTAT_SIM_SUIDSGID) 
						printf(" suid_sgid=0%o",
							(int)fsp->rtree_inode_simu.inodesimu_suidsgid);
					if (a & RULETREE_INODESTAT_SIM_DEVNODE) 
						printf(" device_type=0%o rdev=0x%llX",
							(int)fsp->rtree_inode_simu.inodesimu_devmode,
							(long long)fsp->rtree_inode_simu.inodesimu_rdev);
				} else {
					printf("INODESTAT: <NULL>");
				}
			}
			break;
		case SB2_RULETREE_OBJECT_TYPE_UINT32:
			uip = ruletree_get_pointer_to_uint32(obj_offs);
			if (uip)
				printf("UINT32 %u (0x%X)", *uip, *uip);
			else
				printf("UINT32 <none; got NULL pointer>");
			break;
		case SB2_RULETREE_OBJECT_TYPE_BOOLEAN:
			uip = ruletree_get_pointer_to_boolean(obj_offs);
			if (uip)
				printf("BOOLEAN %s", *uip ? "true" : "false");
			else
				printf("BOOLEAN <none; got NULL pointer>");
			break;
		default:
			printf("<unknown type %d>",
				hdr->rtree_obj_type);
			break;
		}
	} else {
		printf("<invalid value offset>");
	}
}

static void print_ruletree_object_recurse(int indent, const char *name, ruletree_object_offset_t obj_offs)
{
	ruletree_object_hdr_t *hdr;

	hdr = offset_to_ruletree_object_ptr(obj_offs, 0/*any type is ok*/);

	if (hdr) {
		switch (hdr->rtree_obj_type) {
		case SB2_RULETREE_OBJECT_TYPE_CATALOG:
			dump_catalog(obj_offs, name, indent);
			break;
		case SB2_RULETREE_OBJECT_TYPE_OBJECTLIST:
			if (name) {
				print_indent(indent);
				printf("'%s'\n", name);
			}
			dump_objectlist(obj_offs, indent+1);
			break;
		case SB2_RULETREE_OBJECT_TYPE_BINTREE:
			dump_bintree(obj_offs, indent, 1, NULL, NULL);
			break;
		default:
			/* ignore it. */
			break;
		}
	} /* else ignore it. */
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

	if (print_ruletree_offsets) {
		if (rule_dumped[catalog_offs]) {
			print_indent(indent);
			printf("[ => Catalog @ %u '%s']\n", catalog_offs, catalog_name);
			return;
		}
	}
	rule_dumped[catalog_offs] = 1;

	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("Catalog @ %u '%s':\n", catalog_offs, catalog_name);
	} else {
		printf("Catalog '%s':\n", catalog_name);
	}

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
			name = offset_to_ruletree_string_ptr(catp->rtree_cat_name_offs, NULL);
			if (name) {
				printf("'%s'\t", name);
			} else {
				printf("<invalid name offset>\t");
			}
		} else {
			printf("<no name>\t");
		}

		if (catp->rtree_cat_value_offs) {
			print_ruletree_object_type(catp->rtree_cat_value_offs);
		} else {
			printf("<no value>");
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
			name = offset_to_ruletree_string_ptr(catp->rtree_cat_name_offs, NULL);
		}

		if (catp->rtree_cat_value_offs) {
			print_ruletree_object_recurse(indent+1,
				name, catp->rtree_cat_value_offs);

		}
	}
	print_indent(indent);
	if (print_ruletree_offsets) {
		printf("End of Catalog @ %u '%s'.\n", catalog_offs, catalog_name);
	} else {
		printf("End of Catalog '%s'.\n", catalog_name);
	}
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
		case 'o':
			print_ruletree_offsets = 1;
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
	if (attach_ruletree(rule_tree_path, 0) < 0) {
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

