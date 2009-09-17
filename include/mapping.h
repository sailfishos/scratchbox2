/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef MAPPING_H
#define MAPPING_H

#include <sys/types.h>


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

} mapping_results_t;

extern void clear_mapping_results_struct(mapping_results_t *res);
extern void free_mapping_results(mapping_results_t *res);

extern void sbox_map_path(const char *func_name, const char *path,
	int dont_resolve_final_symlink, mapping_results_t *res);

extern void sbox_map_path_at(const char *func_name, int dirfd,
	const char *path, int dont_resolve_final_symlink,
	mapping_results_t *res);

extern void sbox_map_path_for_sb2show(const char *binary_name,
	const char *func_name, const char *path, mapping_results_t *res);

extern void sbox_map_path_for_exec(const char *func_name, const char *path,
	mapping_results_t *res);

extern int sb_execve_preprocess(char **file, char ***argv, char ***envp);
extern char *emumode_map(const char *path);
extern void sb_push_string_to_lua_stack(char *str);
extern char *sb_execve_map_script_interpreter(const char *interpreter,
        const char *interp_arg, const char *mapped_script_filename,
	const char *orig_script_filename, char ***argv, char ***envp);
extern int sb_execve_postprocess(char *exec_type,
	char **mapped_file, char **filename, const char *binary_name,
	char ***argv, char ***envp);

extern char *scratchbox_reverse_path(
	const char *func_name, const char *full_path);

extern const char *fdpathdb_find_path(int fd);

/* ---- internal constants: ---- */

/* "flags", returned from mapping.lua to the C code: */
#define SB2_MAPPING_RULE_FLAGS_READONLY			01
#define SB2_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL	02
#define SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH		04
/* list of all known flags: The preload library will log a warning, if 
 * the mapping code (in Lua) returns unknown flags. This is important
 * because it provides some kind of notification if/when new flags are
 * added in the future, but the preload libraries which are installed to 
 * the targets/tools are not properly updated.
*/
#define SB2_MAPPING_RULE_ALL_FLAGS \
	(SB2_MAPPING_RULE_FLAGS_READONLY | \
	 SB2_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL | \
	 SB2_MAPPING_RULE_FLAGS_FORCE_ORIG_PATH)

#endif
