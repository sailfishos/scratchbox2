/*
 * Copyright (C) 2010 Nokia Corporation.
 * Portion Copyright (C) Lauri Leukkunen <lle@rahina.org>
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 * Author: Lauri T. Aarnio
 *
 * This file contians private defitions and interfaces of sb2's
 * pathmapping subsystem.
*/

#ifndef __PATHMAPPING_INTERNAL_H
#define __PATHMAPPING_INTERNAL_H

#include "rule_tree.h"

/* --------- Path & Path component handling primitives: --------- */

struct path_entry {
	struct path_entry *pe_prev;
	struct path_entry *pe_next;

	int	pe_flags;
	char	*pe_link_dest;	/* used only for symlinks */

	int	pe_path_component_len;

	/* pe_path_component MUST BE the last member of this
	 * struct, this is really a larger string buffer:
	 * pe_path_component[pe_path_component_len+1] */
	char	pe_path_component[1];
};

struct path_entry_list {
	struct path_entry	*pl_first;
	int			pl_flags;
};

#define PATH_FLAGS_ABSOLUTE	01
#define PATH_FLAGS_HAS_TRAILING_SLASH	02
#define PATH_FLAGS_HOST_PATH	04

#define PATH_FLAGS_NOT_SYMLINK	010
#define PATH_FLAGS_IS_SYMLINK	020

#define clear_path_entry_list(p) {memset((p),0,sizeof(*(p)));}

extern void free_path_entries(struct path_entry *work);
extern void free_path_list(struct path_entry_list *listp);

extern void set_flags_in_path_entries(struct path_entry *pep, int flags);
extern char *path_list_to_string(const struct path_entry_list *listp);
extern struct path_entry *split_path_to_path_entries(
	const char *cpath, int *flagsp);

extern char *path_entries_to_string_until(
	const struct path_entry *p_entry,
	const struct path_entry *last_path_entry_to_include,
	int flags);
extern struct path_entry *append_path_entries(
	struct path_entry *head,
	struct path_entry *new_entries);

extern void split_path_to_path_list(
	const char *cpath, struct path_entry_list *listp);
extern struct path_entry *duplicate_path_entries_until(
	const struct path_entry *duplicate_until_this_component,
	const struct path_entry *source_path);
extern void	duplicate_path_list_until(
	const struct path_entry *duplicate_until_this_component,
	struct path_entry_list *new_path_list,
	const struct path_entry_list *source_path_list);
extern struct path_entry *remove_path_entry(
	struct path_entry_list *listp,
	struct path_entry *p_entry);

extern char *path_entries_to_string(
	const struct path_entry *p_entry, int flags);

extern int is_clean_path(struct path_entry_list *listp);

/* --------- Mapping context structure, used for parameter passing --------- */

typedef struct path_mapping_context_s {
	const char		*pmc_binary_name;
	const char		*pmc_func_name;
	uint32_t		pmc_fn_class;
	const char		*pmc_virtual_orig_path;
	int			pmc_dont_resolve_final_symlink;
	struct sb2context	*pmc_sb2ctx;

	/* for paths_ruletree_mapping.c: */
	ruletree_object_offset_t pmc_ruletree_offset;
} path_mapping_context_t;

#define clear_path_mapping_context(p) {memset((p),0,sizeof(*(p)));}

/* ----------- pathmapping_interf.c ----------- */

extern char *reverse_map_path(
	path_mapping_context_t *ctx,
	const char *abs_host_path);

/* ----------- paths_luaif.c ----------- */
extern char *call_lua_function_sbox_translate_path(
	const path_mapping_context_t *ctx,
	int result_log_level,
	const char *abs_clean_virtual_path,
	int *flagsp,
	char **exec_policy_name_ptr);
extern int call_lua_function_sbox_get_mapping_requirements(
	const path_mapping_context_t *ctx,
	const struct path_entry_list *abs_virtual_source_path_list,
	int *min_path_lenp,
	int *call_translate_for_all_p);
extern char *call_lua_function_sbox_reverse_path(
	const path_mapping_context_t *ctx,
	const char *abs_host_path);
extern void drop_rule_from_lua_stack(struct sb2context *sb2ctx);

extern char *clean_and_log_fs_mapping_result(
	const path_mapping_context_t *ctx,
	const char *abs_clean_virtual_path,
	int result_log_level, char *host_path, int flags);

/* ----------- paths_ruletree_mapping.c ----------- */
extern char *ruletree_translate_path(
	const path_mapping_context_t *ctx,
	int result_log_level,
	const char *abs_clean_virtual_path,
	int *flagsp,
	char **exec_policy_name_ptr,
	int *force_fallback_to_lua);
extern int ruletree_get_mapping_requirements(
	path_mapping_context_t *ctx,
	int use_fwd_rules /* a flag */,
	const struct path_entry_list *abs_virtual_source_path_list,
	int *min_path_lenp,
	int *call_translate_for_all_p,
	uint32_t fn_class);

/* ----------- pathresolution.c ----------- */

/* "easy" path cleaning: */
extern void remove_dots_from_path_list(struct path_entry_list *listp);

/* "complex" path cleaning (may call path resolution recursively): */
extern void clean_dotdots_from_path(
	const path_mapping_context_t *ctx,
	struct path_entry_list *abs_path);

extern void sbox_map_path_internal__lua_engine(
	const char *binary_name,
	const char *func_name,
	const char *virtual_orig_path,
	int dont_resolve_final_symlink,
	int process_path_for_exec,
	uint32_t fn_class,
	mapping_results_t *res);

extern void sbox_map_path_internal__c_engine(
	const char *binary_name,
	const char *func_name,
	const char *virtual_orig_path,
	int dont_resolve_final_symlink,
	int process_path_for_exec,
	uint32_t fn_class,
	mapping_results_t *res);

extern char *sbox_reverse_path_internal__c_engine(
        path_mapping_context_t  *ctx,
        const char *abs_host_path);

#endif /* __PATHMAPPING_INTERNAL_H */

