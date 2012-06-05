/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef MAPPING_H
#define MAPPING_H

#include <sys/types.h>
#include <stdint.h>

#include "rule_tree.h"

#define enable_mapping(a) ((a)->mapping_disabled--)
#define disable_mapping(a) ((a)->mapping_disabled++)

/* Mapping results are returned in a structure: */
typedef struct mapping_results_s {

	/* mapping result buffer: This is usually an absolute path.
	 * Note that a fixed-sized array can NOT be used, because the
	 * path mapping engine may produce results that are longer than
	 * PATH_MAX. And this is not just theory, it does happen (some
	 * "configure" scripts create really deep directory structures)
	 * Also note that this may be an empty string, or relative path
	 * after all in some situations.
	*/
	char	*mres_result_buf;

	/* "normalized result", either absolute path or relative.
	 * if the result is absolute, then
	 *	mres_result_path == mres_result_buf
	 * for most of relative results, 
	 *	mres_result_path == mres_result_buf+N
	 * but sometimes this may be separately allocated.
	*/
	char	*mres_result_path;
	int	mres_result_path_was_allocated;

	/* Flag: set if the result has been marked read only */
	int	mres_readonly;

	/* errno: non-zero if an error was detected during
	 * mapping. The interface code should then return
	 * this value to the application (in the "standard"
	 * errno variable) without calling the real function.
	*/
	int	mres_errno;

	/* filled if orig.virtual path was relative: */
	char	*mres_virtual_cwd;

	/* exec policy name */
	const char	*mres_exec_policy_name;	
	/* pointer to allocated exec policy name, freed at destructor
	 * (note that the Lua mapping code allocates it, while the C
	 * mapping code just uses a constant pointer to the shared
	 * memory rule DB. So this is NULL in the latter case) */
	char		*mres_allocated_exec_policy_name;

	const char	*mres_error_text; /* NULL if OK */

	/* set if the C mapping engine failed.
	*/
	const char	*mres_errormsg;
} mapping_results_t;

/* extern void clear_mapping_results_struct(mapping_results_t *res); */
#define clear_mapping_results_struct(res) do{memset((res),0,sizeof(mapping_results_t));}while(0)

extern void free_mapping_results(mapping_results_t *res);

extern void force_path_to_mapping_result(mapping_results_t *res, const char *path);

extern void sbox_map_path(const char *func_name, const char *path,
	int dont_resolve_final_symlink, mapping_results_t *res, uint32_t classmask);

extern void sbox_map_path_at(const char *func_name, int dirfd,
	const char *path, int dont_resolve_final_symlink,
	mapping_results_t *res, uint32_t classmask);

extern char *sbox_virtual_path_to_abs_virtual_path(
        const char *binary_name,
        const char *func_name,
        uint32_t fn_class,
        const char *virtual_orig_path);

extern void sbox_map_path_for_sb2show(const char *binary_name,
	const char *func_name, const char *path, mapping_results_t *res);

extern void sbox_map_path_for_exec(const char *func_name, const char *path,
	mapping_results_t *res);

extern void custom_map_path(const char *binary_name,
	const char *func_name, const char *virtual_path,
	int dont_resolve_final_symlink, uint32_t fn_class,
	mapping_results_t *res, ruletree_object_offset_t rule_list_offset);

extern char *custom_map_abstract_path(
        ruletree_object_offset_t rule_list_offs, const char *binary_name, 
        const char *virtual_orig_path, const char *func_name, 
        int fn_class, const char **new_exec_policy_p);

extern char *emumode_map(const char *path);
#if 0
extern void sb_push_string_to_lua_stack(char *str);
#endif

#if 0
extern int sb_execve_postprocess(const char *exec_type,
	const char *exec_policy_name,
	char **mapped_file, char **filename, const char *binary_name,
	char ***argv, char ***envp);
#endif

extern char *scratchbox_reverse_path(
	const char *func_name, const char *full_path, uint32_t classmask);

extern const char *fdpathdb_find_path(int fd);

extern char *prep_union_dir(const char *dst_path,
		const char **src_paths, int num_real_dir_entries);

/* ---- internal constants: ---- */

/* "flags", Used by both the Lua and the C code:
 *    RULE_FLAGS_READONLY_FS_ALWAYS is same as RULE_FLAGS_READONLY,
 *    but it is easier to have two flags for this because Lua
 *    does not have bit operations; also it enables us to find
 *    rules that still use old syntax
*/
#define SB2_MAPPING_RULE_FLAGS_READONLY			01
#define SB2_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL	02
#define SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH		04
#define SB2_MAPPING_RULE_FLAGS_READONLY_FS_IF_NOT_ROOT	010
#define SB2_MAPPING_RULE_FLAGS_READONLY_FS_ALWAYS	020
#define SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT	040

/* list of all known flags: The preload library will log a warning, if 
 * the mapping code (in Lua) returns unknown flags. This is important
 * because it provides some kind of notification if/when new flags are
 * added in the future, but the preload libraries which are installed to 
 * the targets/tools are not properly updated.
*/
#define SB2_MAPPING_RULE_ALL_FLAGS \
	(SB2_MAPPING_RULE_FLAGS_READONLY | \
	 SB2_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL | \
	 SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH | \
	 SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH_UNLESS_CHROOT | \
	 SB2_MAPPING_RULE_FLAGS_READONLY_FS_IF_NOT_ROOT | \
	 SB2_MAPPING_RULE_FLAGS_READONLY_FS_ALWAYS)

/* Interface classes. 
 * These can be used as conditions in path mapping rules.
 * The interface definition files use these without prefix
 * (SB2_INTERFACE_CLASS_). see interface.master for examples.
 * Note: multiple values can be ORed.
*/
#define SB2_INTERFACE_CLASS_OPEN	0x1
#define SB2_INTERFACE_CLASS_STAT	0x2
#define SB2_INTERFACE_CLASS_EXEC	0x4

#define SB2_INTERFACE_CLASS_SOCKADDR	0x8	/* address in bind, connect */
#define SB2_INTERFACE_CLASS_FTSOPEN	0x10	/* ftsopen */
#define SB2_INTERFACE_CLASS_GLOB	0x20	/* glob */

#define SB2_INTERFACE_CLASS_GETCWD	0x40	/* getcwd() etc */
#define SB2_INTERFACE_CLASS_REALPATH	0x80	/* realpath */
#define SB2_INTERFACE_CLASS_SET_TIMES	0x100	/* utimes() etc: set timestamps */

#define SB2_INTERFACE_CLASS_L10N	0x200	/* gettextdomain etc. */
#define SB2_INTERFACE_CLASS_MKNOD	0x400
#define SB2_INTERFACE_CLASS_RENAME	0x800

#define SB2_INTERFACE_CLASS_PROC_FS_OP	0x1000	/* /proc file system operation */

#define SB2_INTERFACE_CLASS_SYMLINK	0x2000
#define SB2_INTERFACE_CLASS_CREAT	0x4000
#define SB2_INTERFACE_CLASS_REMOVE	0x8000	/* unlink*, remove, rmdir */

#define SB2_INTERFACE_CLASS_CHROOT	0x10000	/* chroot() */

/* interface funtion ->  class(es) mapping table, 
 * created by gen-interface.c */
typedef struct {
	const	char	*fn_name;
	uint32_t	fn_classmask;
} interface_function_and_classes_t;
extern interface_function_and_classes_t interface_functions_and_classes__public[];
extern interface_function_and_classes_t interface_functions_and_classes__private[];

#endif
