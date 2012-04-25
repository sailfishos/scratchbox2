/*
 * Copyright (C) 2012 Nokia Corporation.
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 * Author: Lauri T. Aarnio
 *
 * This file contains private defitions and interfaces of sb2's
 * exec subsystem.
*/

#ifndef __EXEC_INTERNAL_H
#define __EXEC_INTERNAL_H

#include <stddef.h>
#include "rule_tree.h"

extern int apply_exec_preprocessing_rules(char **file, char ***argv, char ***envp);

extern const char *find_exec_policy_name(const char *mapped_path, const char *virtual_path);

/* Exec policies are stored to catalogs in the rule tree;
 * meaning that all values are accessed with strings as keys.
 * (not very optimal, but easy for re-writing the Lua code in C)
 *
 * struct exec_policy_fields is used only for syntax checking:
 * All valid keys for the exec policy catalogs are listed here,
 * so that the C compiler can validate accesses (otherwise typos
 * etc could be left unnoticed)
 * (see the EXEC_POLICY_GET_* macros and exec_policy_get_* functions)
*/
struct exec_policy_fields {
	char log_level;
	char log_message;
	char native_app_ld_so;

	char native_app_ld_library_path;
	char native_app_ld_library_path_prefix;
	char native_app_ld_library_path_suffix;

	char native_app_ld_preload;
	char native_app_ld_preload_prefix;
	char native_app_ld_preload_suffix;

	char native_app_ld_so_rpath_prefix;
	char native_app_ld_so_supports_rpath_prefix;
	char native_app_ld_so_supports_argv0;
	char native_app_ld_so_supports_nodefaultdirs;

	char native_app_locale_path;
	char native_app_gconv_path;

	char exec_flags;

	char script_log_level;
	char script_log_message;
	char script_deny_exec;
	char script_interpreter_rules;
	char script_set_argv0_to_mapped_interpreter;
};

typedef struct {
	ruletree_object_offset_t exec_policy_offset;
} exec_policy_handle_t;

#define exec_policy_handle_is_valid(eph) ((eph).exec_policy_offset != 0)

/* Use CPU transparency even if binaries are compatible with host */
#define SB2_EXEC_FLAGS_FORCE_CPU_TRANSPARENCY	1

extern exec_policy_handle_t	find_exec_policy_handle(const char *policyname);

extern const char *exec_policy_get_string(exec_policy_handle_t eph,
	const char *s_name, size_t fldoffs);
extern int exec_policy_get_boolean(exec_policy_handle_t eph,
	const char *b_name, size_t fldoffs);
extern int exec_policy_get_uint32(exec_policy_handle_t eph,
	const char *u_name, size_t fldoffs);
extern ruletree_object_offset_t exec_policy_get_rules(exec_policy_handle_t eph,
	const char *r_name, size_t fldoffs);

#define EXEC_POLICY_GET_STRING(eph, fieldname) \
	exec_policy_get_string(eph, #fieldname, \
		offsetof(struct exec_policy_fields, fieldname))

#define EXEC_POLICY_GET_BOOLEAN(eph, fieldname) \
	exec_policy_get_boolean(eph, #fieldname, \
		offsetof(struct exec_policy_fields, fieldname))

#define EXEC_POLICY_GET_UINT32(eph, fieldname) \
	exec_policy_get_uint32(eph, #fieldname, \
		offsetof(struct exec_policy_fields, fieldname))

#define EXEC_POLICY_GET_RULES(eph, fieldname) \
	exec_policy_get_rules(eph, #fieldname, \
		offsetof(struct exec_policy_fields, fieldname))

extern int exec_map_script_interpreter(
	exec_policy_handle_t    eph,
	const char *exec_policy_name,
	const char *interpreter,
	const char *interp_arg, 
	const char *mapped_script_filename,
	const char *orig_script_filename,
	char    **argv,
	const char **new_exec_policy_p, 
        char       **mapped_interpreter_p);

extern int exec_postprocess_native_executable(
        const char *exec_policy_name,
        char **mapped_file,
        char **filename,
        const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp);

extern int exec_postprocess_cpu_transparency_executable(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp,
	const char *conf_cputransparency_name);

extern int exec_postprocess_host_static_executable(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp);

#endif /* __EXEC_INTERNAL_H */

