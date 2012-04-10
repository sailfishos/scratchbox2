/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include "sb2_execs.h"

/* Exec postprocessing for native, dynamically linked binaries.
 * exec_postprocess_native_executable() is called to decide HOW the executable
 * should be started (see description of the algorithm in sb_exec.c)
*/

static int elem_count(const char **elems)
{
        int count = 0;
        char **p = (char **)elems;
        while (*p) {
                p++; count++;
        }
        return count;
}

/* used for building new argv[] and envp[] vectors */
struct strv_s {
	const char	*strv_name;

	const char	**strv_orig_v;
	int		strv_num_orig_elems;

	const char	**strv_new_v;
	int		strv_first_free_idx;
	int		strv_new_v_max_size;
};

static void init_strv(struct strv_s *svp, const char *name,
	const char **orig_v, int num_additional_elems)
{
	int	num_orig_elems = elem_count(orig_v);

	svp->strv_name = name;
	svp->strv_num_orig_elems = num_orig_elems;
	svp->strv_orig_v = orig_v;
	svp->strv_new_v_max_size = num_orig_elems + num_additional_elems;
	/* add one to 'nmemb' for the terminating NULL */
	svp->strv_new_v = calloc(svp->strv_new_v_max_size + 1, sizeof(char*));
	svp->strv_first_free_idx = 0;
}

static void add_string_to_strv(struct strv_s *svp, const char *str)
{
	SB_LOG(SB_LOGLEVEL_NOISE,
		"%s: %s[%d] = '%s'",
		__func__, svp->strv_name, svp->strv_first_free_idx, str);
	assert(svp->strv_first_free_idx < svp->strv_new_v_max_size);
	(svp->strv_new_v)[svp->strv_first_free_idx] = str;
	(svp->strv_first_free_idx)++;
}

static const char *add_string_from_ruletreelist_to_strv(
	ruletree_object_offset_t ruletreelist_offs,
	uint32_t idx,
	struct strv_s *svp,
	const char *conf_cputransparency_name)
{
	ruletree_object_offset_t	ofs = 0;
	const char			*cp = NULL;

	ofs = ruletree_objectlist_get_item(ruletreelist_offs, idx);
	if (ofs) cp = offset_to_ruletree_string_ptr(ofs, NULL);
	if (!cp) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Configuration error: Faulty qemu %s element (%d, %s)",
			svp->strv_name, idx, conf_cputransparency_name);
		return(NULL);
	}
	add_string_to_strv(svp, cp);
	return(cp);
}

/* FIXME: combine "get_users_ld_preload" and
 * "get_users_ld_library_path" !
*/
static const char *get_users_ld_preload(const char **orig_env)
{
	int	i;
	const char *n = "__SB2_LD_PRELOAD=";
	int	sz = strlen(n);

	for (i = 0; orig_env[i]; i++) {
		if (!strncmp(orig_env[i], n, sz)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: found '%s'",
				__func__, orig_env[i]+sz);
			return(orig_env[i] + sz);
		}
	}
	return(NULL);
}

static const char *get_users_ld_library_path(const char **orig_env)
{
	int	i;
	const char *n = "__SB2_LD_LIBRARY_PATH=";
	int	sz = strlen(n);

	for (i = 0; orig_env[i]; i++) {
		if (!strncmp(orig_env[i], n, sz)) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: found '%s'",
				__func__, orig_env[i]+sz);
			return(orig_env[i] + sz);
		}
	}
	return(NULL);
}

static void setenv_native_app_ld_preload(
	exec_policy_handle_t	eph,
	struct strv_s *new_envp)
{
	const char *new_path = NULL;
	const char *native_app_ld_preload = NULL;

	native_app_ld_preload = EXEC_POLICY_GET_STRING(eph, native_app_ld_preload);
	if (native_app_ld_preload) {
		new_path = native_app_ld_preload;
	} else {
		const char *native_app_ld_preload_prefix = NULL;
		const char *native_app_ld_preload_suffix = NULL;
		const char *libpath = get_users_ld_preload(new_envp->strv_orig_v);

		native_app_ld_preload_prefix = EXEC_POLICY_GET_STRING(eph,
			native_app_ld_preload_prefix);
		native_app_ld_preload_suffix = EXEC_POLICY_GET_STRING(eph,
			native_app_ld_preload_suffix);
		if (native_app_ld_preload_prefix ||
		    native_app_ld_preload_suffix) {
#define str_not_empty(s) ((s) && *(s))
			char *cp;
			if (libpath) {
				assert(asprintf(&cp, "LD_PRELOAD=%s%s%s%s%s",
					(str_not_empty(native_app_ld_preload_prefix) ?
					 native_app_ld_preload_prefix : ""), /* 1 */
					(str_not_empty(native_app_ld_preload_prefix) ? ":" : ""), /* 2 */
					libpath, /* 3 */
					(str_not_empty(libpath) && str_not_empty(native_app_ld_preload_suffix) ?
					 ":" : ""), /* 4 */
					(str_not_empty(native_app_ld_preload_suffix) ?
					 native_app_ld_preload_suffix : "") /* 5 */
					) > 0);	
			} else {
				/* no libpath */
				assert(asprintf(&cp, "LD_PRELOAD=%s%s%s",
					(str_not_empty(native_app_ld_preload_prefix) ?
					 native_app_ld_preload_prefix : ""), /* 1 */
					(str_not_empty(native_app_ld_preload_prefix) &&
					 str_not_empty(native_app_ld_preload_suffix) ? ":" : ""), /* 2 */
					(str_not_empty(native_app_ld_preload_suffix) ?
					 native_app_ld_preload_suffix : "") /* 3 */
					) > 0);	
			}
			new_path = cp;
		} else {
			new_path = NULL;
		}
	}

	if (new_path) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: set LD_PRELOAD='%s'",
			__func__, new_path);
	} else {
		char *cp;
		new_path = ruletree_catalog_get_string("config", "host_ld_preload");
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: No value for LD_PRELOAD, using host's variable '%s'",
			__func__, new_path);
		assert(asprintf(&cp, "LD_PRELOAD=%s", new_path) > 0);
		new_path = cp;
	}
	add_string_to_strv(new_envp, new_path);
}

/* Old Lua code for reference:
 *
 *function setenv_native_app_ld_library_path(exec_policy, envp)
 *        local new_path
 *
 *        if (exec_policy.native_app_ld_library_path ~= nil) then
 *                -- attribute "native_app_ld_library_path" overrides everything else:
 *                new_path = exec_policy.native_app_ld_library_path
 *        elseif ((exec_policy.native_app_ld_library_path_prefix ~= nil) or
 *                (exec_policy.native_app_ld_library_path_suffix ~= nil)) then
 *                -- attributes "native_app_ld_library_path_prefix" and
 *                -- "native_app_ld_library_path_suffix" extend user's value:
 *                local libpath = get_users_ld_library_path(envp)
 *                new_path = join_paths(
 *                        exec_policy.native_app_ld_library_path_prefix,
 *                        join_paths(libpath,
 *                                exec_policy.native_app_ld_library_path_suffix))
 *        else
 *                new_path = nil
 *        end
 *
 *        -- Set the value:
 *        if (new_path == nil) then
 *                if debug_messages_enabled then
 *                        sb.log("debug", "No value for LD_LIBRARY_PATH, using host's path")
 *                end
 *                -- Use host's original value
 *                new_path = host_ld_library_path
 *        end
 *
 *        set_ld_library_path(envp, new_path)
 *        return true
 *end
*/
/* FIXME: combine "setenv_native_app_ld_library_path" and
 * "setenv_native_app_ld_preload" !
*/
static void setenv_native_app_ld_library_path(
	exec_policy_handle_t	eph,
	struct strv_s *new_envp)
{
	const char *new_path = NULL;
	const char *native_app_ld_library_path = NULL;

	native_app_ld_library_path = EXEC_POLICY_GET_STRING(eph, native_app_ld_library_path);
	if (native_app_ld_library_path) {
		new_path = native_app_ld_library_path;
	} else {
		const char *native_app_ld_library_path_prefix = NULL;
		const char *native_app_ld_library_path_suffix = NULL;
		const char *libpath = get_users_ld_library_path(new_envp->strv_orig_v);

		native_app_ld_library_path_prefix = EXEC_POLICY_GET_STRING(eph,
			native_app_ld_library_path_prefix);
		native_app_ld_library_path_suffix = EXEC_POLICY_GET_STRING(eph,
			native_app_ld_library_path_suffix);
		if (native_app_ld_library_path_prefix ||
		    native_app_ld_library_path_suffix) {
#define str_not_empty(s) ((s) && *(s))
			char *cp;
			if (libpath) {
				assert(asprintf(&cp, "LD_LIBRARY_PATH=%s%s%s%s%s",
					(str_not_empty(native_app_ld_library_path_prefix) ?
					 native_app_ld_library_path_prefix : ""), /* 1 */
					(str_not_empty(native_app_ld_library_path_prefix) ? ":" : ""), /* 2 */
					libpath, /* 3 */
					(str_not_empty(libpath) && str_not_empty(native_app_ld_library_path_suffix) ?
					 ":" : ""), /* 4 */
					(str_not_empty(native_app_ld_library_path_suffix) ?
					 native_app_ld_library_path_suffix : "") /* 5 */
					) > 0);	
			} else {
				/* no libpath */
				assert(asprintf(&cp, "LD_LIBRARY_PATH=%s%s%s",
					(str_not_empty(native_app_ld_library_path_prefix) ?
					 native_app_ld_library_path_prefix : ""), /* 1 */
					(str_not_empty(native_app_ld_library_path_prefix) &&
					 str_not_empty(native_app_ld_library_path_suffix) ? ":" : ""), /* 2 */
					(str_not_empty(native_app_ld_library_path_suffix) ?
					 native_app_ld_library_path_suffix : "") /* 3 */
					) > 0);	
			}
			new_path = cp;
		} else {
			new_path = NULL;
		}
	}

	if (new_path) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: set LD_LIBRARY_PATH='%s'",
			__func__, new_path);
	} else {
		char *cp;
		new_path = ruletree_catalog_get_string("config", "host_ld_library_path");
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: No value for LD_LIBRARY_PATH, using host's path '%s'",
			__func__, new_path);
		assert(asprintf(&cp, "LD_LIBRARY_PATH=%s", new_path) > 0);
		new_path = cp;
	}
	add_string_to_strv(new_envp, new_path);
}

/* "generic part" (compare with Lua:sb_execve_postprocess() */
static int exec_postprocess_prepare(
	const char *exec_policy_name,
	exec_policy_handle_t *ephp,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
	struct strv_s	*new_argv,
	int	add_argv_size,
	const char **orig_env,
	struct strv_s	*new_envp,
	int	add_envp_size)
{
	exec_policy_handle_t	eph;

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: mapped_file=%s filename=%s binary_name=%s exec_policy_name=%s",
		__func__, *mapped_file, *filename, binary_name, exec_policy_name);

	*ephp = eph = find_exec_policy_handle(exec_policy_name);
	if (!exec_policy_handle_is_valid(eph)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: invalid exec_policy_handle, allow direct exec",
			__func__);
		return(1);
	}
	
	{
		const char		*log_level = NULL;

		log_level = EXEC_POLICY_GET_STRING(eph, log_level);
		if (log_level) {
			const char	*log_message;

			log_message = EXEC_POLICY_GET_STRING(eph, log_message);
			SB_LOG(sblog_level_name_to_number(log_level), "%s", log_message);
		}
	}

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: Applying exec_policy '%s'",
		__func__, exec_policy_name);

	/* allocate new environment.
	 * reserve space for new entries, plus __SB2_EXEC_POLICY_NAME
	*/
	init_strv(new_envp, "envp", orig_env, 1 + add_envp_size);
	{
		char	*cp;

		assert(asprintf(&cp, "__SB2_EXEC_POLICY_NAME=%s", exec_policy_name) > 0);
		add_string_to_strv(new_envp, cp);
	}

	init_strv(new_argv, "argv", orig_argv, add_argv_size);

	/* --- end of "generic part" */
	return(0);
}

/* A very straightforward conversion from Lua.
 * (old Lua code is included in the comments for
 * reference, and also for comparing functionality)
 *
 * Same result code as Lua returned, the return value:
 *    -1 = do not execute.
 *    0 = argc&argv were updated, OK to execute with the new params
 *    1 = ok to exec directly with orig.arguments
 *
 * (replacement for both sb_execve_postprocess() (C) and
 *  sb_execve_postprocess (Lua))
*/
int exec_postprocess_native_executable(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp)
{
	exec_policy_handle_t	eph;
	const char		*native_app_ld_so = NULL;
	char			*new_filename = *filename;
	char			*new_mapped_file = *mapped_file;
	int			first_argv_element_to_copy = 0;
	struct strv_s		new_envp;
	struct strv_s		new_argv;

	/* Prepare. For the new argv, allocate room for 
	 * for optional new entries, needed if indirect startup:
	 *  1) ld.so path
	 *  2) "--rpath-prefix" or  "--inhibit-rpath"
	 *  3) exec_policy.native_app_ld_so_rpath_prefix or ""
	 *  4) "--nodefaultdirs" (optional)
	 *  5) "--argv0" (optional)
	 *  6) value for argv0 (optional)
	 * For new envp, reserve extra space for
	 *  1) LD_LIBRARY_PATH
	 *  2) LD_PRELOAD
	 *  3) LOCPATH (optional)
	 *  4) NLSPATH (optional)
	 *  5) GCONV_PATH (optional)
	*/
	if (exec_postprocess_prepare(exec_policy_name, &eph, mapped_file,
		filename, binary_name, orig_argv, &new_argv, 6,
		orig_env, &new_envp, 5)) return(1);

	/* Old Lua code, for reference:
	 *	function sb_execve_postprocess_native_executable(exec_policy,
	 *		exec_type, mapped_file, filename, argv, envp)
	 *
	 *		-- Native binary. See what we need to do with it...
	 *		if debug_messages_enabled then
	 *			sb.log("debug", "sb_execve_postprocess_native_executable")
	 *		end
	 *
	 *		local new_argv = {}
	 *		local new_envp = envp
	 *		local new_filename = filename
	 *		local new_mapped_file = mapped_file
	 *		-- by default, copy argv from index 1 (refers to argv[0])
	 *		local first_argv_element_to_copy = 1
	 *		local updated_args = 0
	 *
	*/

	/* Lua:
	 *	if (exec_policy.native_app_ld_so ~= nil) then
	 *		-- we need to use ld.so for starting the binary, 
	 *		-- instead of starting it directly:
	 *		new_mapped_file = exec_policy.native_app_ld_so
	 *		table.insert(new_argv, exec_policy.native_app_ld_so)
	*/
	native_app_ld_so = EXEC_POLICY_GET_STRING(eph, native_app_ld_so);
	if (!native_app_ld_so) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: No native_app_ld_so", __func__);
	} else {
		const char *native_app_ld_so_rpath_prefix = NULL;

		/* we need to use ld.so for starting the binary, 
		 * instead of starting it directly: */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: native_app_ld_so='%s'",
			__func__, native_app_ld_so);

		add_string_to_strv(&new_argv, native_app_ld_so);
		new_mapped_file = strdup(native_app_ld_so); /* FIXME */

		/* Ignore RPATH and RUNPATH information:
		 * This will prevent accidental use of host's libraries,
		 * if the binary has been set to use RPATHs. 
		 * (it would be nice if we could log warnings about them,
		 * but currently there is no easy way to do that)
		 *
		 * Lua:
		 *	if (exec_policy.native_app_ld_so_supports_rpath_prefix) then
		 *		table.insert(new_argv, "--rpath-prefix")
		 *		table.insert(new_argv, exec_policy.native_app_ld_so_rpath_prefix)
		 *	else
		 *		table.insert(new_argv, "--inhibit-rpath")
		 *		table.insert(new_argv, "") -- empty "LIST" == the binary itself
		 *	end
		*/
		native_app_ld_so_rpath_prefix = EXEC_POLICY_GET_STRING(eph, native_app_ld_so_rpath_prefix);
		if (native_app_ld_so_rpath_prefix &&
		    EXEC_POLICY_GET_BOOLEAN(eph, native_app_ld_so_supports_rpath_prefix)) {
			add_string_to_strv(&new_argv, "--rpath-prefix");
			add_string_to_strv(&new_argv, native_app_ld_so_rpath_prefix);
		} else {
			add_string_to_strv(&new_argv, "--inhibit-rpath");
			add_string_to_strv(&new_argv, "");
		}

		/* Lua:
		 *	if (exec_policy.native_app_ld_so_supports_nodefaultdirs) then
		 *		table.insert(new_argv, "--nodefaultdirs")
		 *	end
		*/
		if (EXEC_POLICY_GET_BOOLEAN(eph, native_app_ld_so_supports_nodefaultdirs)) {
			add_string_to_strv(&new_argv, "--nodefaultdirs");
		}

		/*
		 * NOTE/WARNING: The default ld.so (ld-linux.so) will loose
		 * argv[0], when the binary is executed by ld.so's
		 * command line (which we will be doing). It will always copy 
		 * the filename to argv[0].
		 *
		 * We now have a patch for ld.so which introduces a new
		 * option, "--argv0 argument", and a flag is used to tell
		 * if a patched ld.so is available (the "sb2" script finds 
		 * that out during startup phase).
		 *
		 * Lua:
		 *	if (exec_policy.native_app_ld_so_supports_argv0) then
		 *		table.insert(new_argv, "--argv0")
		 *		-- C's argv[0] is in argv[1] here!
		 *		table.insert(new_argv, argv[1])
		 *		table.insert(new_argv, mapped_file)
		 *	else
		 *		-- Replace argv[0] by pathname:
		 *		table.insert(new_argv, mapped_file)
		 *	end
		 *	first_argv_element_to_copy = 2 -- NOTE: In Lua, argv[1] is the 1st one
		 *
		 *	updated_args = 1
		 * end
		*/
		if (EXEC_POLICY_GET_BOOLEAN(eph, native_app_ld_so_supports_argv0)) {
			add_string_to_strv(&new_argv, "--argv0");
			add_string_to_strv(&new_argv, orig_argv[0]);
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: argv: add '%s'", __func__, *mapped_file);
		add_string_to_strv(&new_argv, *mapped_file);
		first_argv_element_to_copy = 1; /* in C, argv[0] is the first one */
	}

	/* Ensure that the LD_LIBRARY_PATH and LD_PRELOAD are set,
	 * SB2 won't work without any.
	 *
	 * Lua:
	 *	if setenv_native_app_ld_library_path(exec_policy, new_envp) then
	 *		updated_args = 1
	 *	end
	 *	-- Also, set that LD_PRELOAD
	 *	if setenv_native_app_ld_preload(exec_policy, new_envp) then
	 *		updated_args = 1
	 *	end
	*/
	setenv_native_app_ld_library_path(eph, &new_envp); 
	setenv_native_app_ld_preload(eph, &new_envp);

	/* When exec_policy contains field 'native_app_locale_path' we
	 * need to set environment variables $LOCPATH (and $NLSPATH) to
	 * point there.  Localization functions (e.g isalpha(), etc.)
	 * gets their locale specific information from $LOCPATH when
	 * it is set.
	 *
	 * Lua:
	 *	if exec_policy.native_app_locale_path ~= nil then
	 *		if debug_messages_enabled then
	 *			sb.log("debug", string.format("setting LOCPATH=%s",
	 *			    exec_policy.native_app_locale_path))
	 *		end
	 *		table.insert(new_envp, "LOCPATH=" ..
	 *		    exec_policy.native_app_locale_path)
	 *		table.insert(new_envp, "NLSPATH=" ..
	 *		    exec_policy.native_app_locale_path)
	 *		updated_args = 1
	 *	end
	*/
	{
		const char *native_app_locale_path;

		native_app_locale_path = EXEC_POLICY_GET_STRING(eph, native_app_locale_path);
		if (native_app_locale_path) {
			char	*cp;

			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: setting LOCPATH and NLSPATH to '%s'",
				__func__, native_app_locale_path);
			assert(asprintf(&cp, "LOCPATH=%s", native_app_locale_path) > 0);
			add_string_to_strv(&new_envp, cp);
			assert(asprintf(&cp, "NLSPATH=%s", native_app_locale_path) > 0);
			add_string_to_strv(&new_envp, cp);
		}
	}

	/* Lua:
	 *	if exec_policy.native_app_gconv_path ~= nil then
	 *		if debug_messages_enabled then
	 *			sb.log("debug", string.format("setting GCONV_PATH=%s",
	 *			    exec_policy.native_app_gconv_path))
	 *		end
	 *		table.insert(new_envp, "GCONV_PATH=" ..
	 *		    exec_policy.native_app_gconv_path)
	 *		updated_args = 1
	 *	end
	*/
	{
		const char *native_app_gconv_path;

		native_app_gconv_path = EXEC_POLICY_GET_STRING(eph, native_app_gconv_path);
		if (native_app_gconv_path) {
			char	*cp;

			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: setting GCONV_PATH to '%s'",
				__func__, native_app_gconv_path);
			assert(asprintf(&cp, "GCONV_PATH=%s", native_app_gconv_path) > 0);
			add_string_to_strv(&new_envp, cp);
		}
	}
		
	/* add rest of orig.env. to new_new */
	{
		int i;
		const char *n_ld_library_path = "LD_LIBRARY_PATH=";
		int	sz_ld_library_path = strlen(n_ld_library_path);
		const char *n_ld_preload = "LD_PRELOAD=";
		int	sz_ld_preload = strlen(n_ld_preload);

		/* FIXME: skip GCONV_PATH, NLSPATH, LOCPATH if
		 * those were set by exec policy.
		 * but Lua code didn't do that, so this won't
		 * do that in the first version either.
		*/

		for (i = 0; orig_env[i]; i++) {
			switch (orig_env[i][0]) {
			case 'L':
				if (!strncmp(orig_env[i], n_ld_library_path, sz_ld_library_path) ||
				    !strncmp(orig_env[i], n_ld_preload, sz_ld_preload)) {
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"%s: env: skip '%s'", __func__, orig_env[i]);
					continue;
				}
				break;
			}
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: env: add '%s'", __func__, orig_env[i]);
			add_string_to_strv(&new_envp, orig_env[i]);
		}
	}

	/* add rest of orig.args. to new_args.
	 * Lua had "updated_args" flag, but here we set all
	 * variables always.
	 *
	 * Lua:
	 *	if (updated_args == 1) then
	 *		-- Add components from original argv[]
	 *		local i
	 *		for i = first_argv_element_to_copy, table.maxn(argv) do
	 *			table.insert(new_argv, argv[i])
	 *		end
	 *
	 *		return 0, new_mapped_file, new_filename, #new_argv, new_argv, #new_envp, new_envp
	 *	end
	 *
	 *	-- else args not modified.
	 *	return 1, mapped_file, filename, #argv, argv, #envp, envp
	 * end
	*/
	{
		int i;

		for (i = first_argv_element_to_copy; orig_argv[i]; i++) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: argv: add '%s'", __func__, orig_argv[i]);
			add_string_to_strv(&new_argv, orig_argv[i]);
		}
	}

	*set_envp = new_envp.strv_new_v;
	*set_argv = new_argv.strv_new_v;
	*mapped_file = new_mapped_file;
	*filename = new_filename;
	/* instruct caller to always use argv,envp,filename
	 * and mapped file from this routine */
	return(0);
}

static int test_cputransp_boolean(const char **namev)
{
	ruletree_object_offset_t	ofs = 0;

	ofs = ruletree_catalog_vget(namev);
	if (ofs) {
		uint32_t	*boolp;

		boolp = ruletree_get_pointer_to_boolean(ofs);
		if (boolp && *boolp) return(1);
	}
	return(0);
}

static const char *get_cputransp_string(const char **namev)
{
	ruletree_object_offset_t	ofs = 0;

	ofs = ruletree_catalog_vget(namev);
	if (ofs) {
		return(offset_to_ruletree_string_ptr(ofs, NULL));
	}
	return(NULL);
}

static int matches_gconv_path_nlspath_or_locpath(const char *cp)
{
	const char *gconv_path_prefix = "GCONV_PATH=";
	const char *nlspath_prefix = "NLSPATH=";
	const char *locpath_prefix = "LOCPATH=";
	const int gconv_path_prefix_len = strlen(gconv_path_prefix);
	const int nlspath_prefix_len = strlen(nlspath_prefix);
	const int locpath_prefix_len = strlen(locpath_prefix);

	if (!strncmp(cp, gconv_path_prefix, gconv_path_prefix_len) ||
	    !strncmp(cp, nlspath_prefix, nlspath_prefix_len) ||
	    !strncmp(cp, locpath_prefix, locpath_prefix_len)) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "matches '%s'", cp);
		return(1);
	}
	return(0);
}

/* CPU transparency with Qemu:
 *
 * Another very straightforward conversion from Lua.
 * (old Lua code is included in the comments for
 * reference, and also for comparing functionality)
 *
 * Same result code as Lua returned, the return value:
 *    -1 = do not execute.
 *    0 = argc&argv were updated, OK to execute with the new params
 *    1 = ok to exec directly with orig.arguments
*/
static int exec_postprocess_qemu(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp,
	const char *conf_cputransparency_name)
{
	exec_policy_handle_t	eph;
	char			*new_filename = *filename;
	char			*new_mapped_file = *mapped_file;
	struct strv_s		new_envp;
	struct strv_s		new_argv;
	const char		*namev_in_ruletree[4];
	ruletree_object_offset_t	qemu_argv_list_offs = 0;
	uint32_t			qemu_argv_list_size = 0;
	ruletree_object_offset_t	qemu_env_list_offs = 0;
	uint32_t			qemu_env_list_size = 0;
	const char	*ld_trace_prefix = "LD_TRACE_";
	const int	ld_trace_prefix_len = strlen(ld_trace_prefix);
	const char	*sb2_ld_preload_prefix = "__SB2_LD_PRELOAD=";
	const int	sb2_ld_preload_prefix_len = strlen(sb2_ld_preload_prefix);
	int		num_ld_trace_env_vars = 0;

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: postprocess '%s' '%s'", __func__, *mapped_file, *mapped_file);

	namev_in_ruletree[0] = "cputransparency";
	namev_in_ruletree[1] = conf_cputransparency_name;
	namev_in_ruletree[2] = NULL; /* this will be varied below */
	namev_in_ruletree[3] = NULL;
	
	/* get number of additional argv elements for Qemu: */
	namev_in_ruletree[2] = "qemu_argv";
	qemu_argv_list_offs = ruletree_catalog_vget(namev_in_ruletree);
	if (qemu_argv_list_offs)
		qemu_argv_list_size = ruletree_objectlist_get_list_size(
			qemu_argv_list_offs);
	/* and additional envp elements for Qemu: */
	namev_in_ruletree[2] = "qemu_env";
	qemu_env_list_offs = ruletree_catalog_vget(namev_in_ruletree);
	if (qemu_env_list_offs)
		qemu_env_list_size = ruletree_objectlist_get_list_size(
			qemu_env_list_offs);
	/* count number of LD_TRACE_ variables in environment */
	{
		int i;
		for (i = 0; orig_env[i]; i++) {
			const char *orig_env_var = orig_env[i];

			if (*orig_env_var == 'L') {
				if (!strncmp(orig_env_var, ld_trace_prefix,
					ld_trace_prefix_len)) {
					num_ld_trace_env_vars++;
				}
			}
		}
	}
	
	/* Preparations.
	 * Following arguments may be added for Qemu, so reserve
	 * additional argv slots:
	 *  1) "-L"
	 *  2) "/"
	 *  3) "-0"
	 *  4) argv[0] value
	 *  5) "-libattr-hack"
	 *  6) 2 * num_ld_trace_env_vars
	 * addtions to env:
	 *  1) LD_PRELOAD
	 *  2) LD_LIBRARY_PATH
	*/
	if (exec_postprocess_prepare(exec_policy_name, &eph, mapped_file,
		filename, binary_name, orig_argv,
		&new_argv, qemu_argv_list_size + 5 + 2*num_ld_trace_env_vars,
		orig_env, &new_envp, qemu_env_list_size + 2))
			return(-1);

	/* Old Lua code, for reference:
	 *function sb_execve_postprocess_cpu_transparency_executable(exec_policy,
	 *    exec_type, mapped_file, filename, argv, envp, conf_cputransparency)
	 *
	 *	if debug_messages_enabled then
	 *		sb.log("debug", "postprocessing cpu_transparency for " .. filename)
	 *	end
	*/
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: postprocessing cpu_transparency for '%s'", __func__, *filename);

	/*	if conf_cputransparency.method_is_qemu then
	 *		local new_envp = {}
	 *		local new_argv = {}
	 *		local new_filename
	 *
	 *		if conf_cputransparency.qemu_argv == nil then
	 *			table.insert(new_argv, conf_cputransparency.cmd)
	 *			new_filename = conf_cputransparency.cmd
	 *		else
	 *			for i = 1, table.maxn(conf_cputransparency.qemu_argv) do
	 *				table.insert(new_argv, conf_cputransparency.qemu_argv[i])
	 *			end
	 *			new_filename = conf_cputransparency.qemu_argv[1]
	 *		end
	*/
	if (qemu_argv_list_size == 0) {
		const char	*cputransparency_cmd = NULL;

		namev_in_ruletree[2] = "cmd";
		cputransparency_cmd = get_cputransp_string(namev_in_ruletree);
		if (!cputransparency_cmd) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: No command for cpu_transparency (%s)", __func__,
				conf_cputransparency_name);
			return(-1); /* do not execute */
		}
		add_string_to_strv(&new_argv, cputransparency_cmd);
		new_mapped_file = strdup(cputransparency_cmd);
	} else {
		uint32_t i;
		for (i = 0; i < qemu_argv_list_size; i++) {
			const char *cp = NULL;

			cp = add_string_from_ruletreelist_to_strv(
				qemu_argv_list_offs, i, &new_argv,
				conf_cputransparency_name);
			if (!cp) return(-1); /* do not execute */
			if (i == 0) {
				new_mapped_file = strdup(cp);
			}
		}
	}

	/*		if conf_cputransparency.qemu_env ~= nil then
	 *			for i = 1, table.maxn(conf_cputransparency.qemu_env) do
	 *				table.insert(new_envp, conf_cputransparency.qemu_env[i])
	 *			end
	 *		end
	*/
	if (qemu_env_list_offs) {
		uint32_t i;
		for (i = 0; i < qemu_env_list_size; i++) {
			const char *cp = NULL;

			cp = add_string_from_ruletreelist_to_strv(
				qemu_env_list_offs, i, &new_envp,
				conf_cputransparency_name);
			if (!cp) return(-1); /* do not execute */
		}
	}

	/*		-- target runtime linker comes from /
	 *		table.insert(new_argv, "-L")
	 *		table.insert(new_argv, "/")
	*/
	add_string_to_strv(&new_argv, "-L");
	add_string_to_strv(&new_argv, "/");

	/* 		if conf_cputransparency.has_argv0_flag then
	 *			-- set target argv[0]
	 *			table.insert(new_argv, "-0")
	 *			table.insert(new_argv, argv[1])
	 *		end
	*/
	namev_in_ruletree[2] = "has_argv0_flag";
	if (test_cputransp_boolean(namev_in_ruletree)) {
		add_string_to_strv(&new_argv, "-0");
		add_string_to_strv(&new_argv, orig_argv[0]);
	}

	/*		if conf_cputransparency.qemu_has_libattr_hack_flag then
	 *			-- For ARM emulation:
	 *			-- a nasty bug exists in some older libattr library
	 *			-- version (e.g. it causes "ls -l" to crash), this
	 *			-- flag enables a hack in Qemu which makes
	 *			-- libattr to work correctly even if it uses incorrect
	 *			-- system call format.
	 *			table.insert(new_argv, "-libattr-hack")
	 *		end
	*/
	namev_in_ruletree[2] = "qemu_has_libattr_hack_flag";
	if (test_cputransp_boolean(namev_in_ruletree)) {
		add_string_to_strv(&new_argv, "-libattr-hack");
	}

	/*		if conf_cputransparency.qemu_has_env_control_flags then
	 *			for i = 1, #envp do
	 *				-- drop LD_TRACE_* from target environment
	 *				if string.match(envp[i], "^LD_TRACE_.*") then
	 *					-- .. and move to qemu command line 
	 *					table.insert(new_argv, "-E")
	 *					table.insert(new_argv, envp[i])
	 *				elseif string.match(envp[i], "^__SB2_LD_PRELOAD=.*") then
	 *					-- FIXME: This will now drop application's
	 *					-- LD_PRELOAD. This is not really what should 
	 *					-- be done... To Be Fixed.
	 *				else
	 *					table.insert(new_envp, envp[i])
	 *				end
	 *			end
	 *		else
	 *			-- copy environment. Some things will be broken with
	 *			-- this qemu (for example, prelinking won't work, etc)
	 *			new_envp = envp
	 *		end
	*/
	namev_in_ruletree[2] = "qemu_has_env_control_flags";
	if (test_cputransp_boolean(namev_in_ruletree)) {
		int i;
		for (i = 0; i < new_envp.strv_num_orig_elems; i++) {
			const char *orig_env_var = orig_env[i];

			switch (*orig_env_var) {
			case 'G':
			case 'N':
			case 'L':
				/* drop GCONV_PATH, NLSPATH and LOCPATH
				 * completely */ 
				if (matches_gconv_path_nlspath_or_locpath(orig_env_var)) {
					continue;
				}
				/* drop LD_TRACE_* from target environment,
				 * and move to it qemu command line */
				if (!strncmp(orig_env_var, ld_trace_prefix,
					ld_trace_prefix_len)) {
					add_string_to_strv(&new_argv, "-E");
					add_string_to_strv(&new_argv, orig_env_var);
					continue;
				}
				break;
			case '_':
				if (!strncmp(orig_env_var, sb2_ld_preload_prefix,
					sb2_ld_preload_prefix_len)) {
					/* FIXME: This will now drop application's
					 * LD_PRELOAD. This is not really what should 
					 * be done... To Be Fixed.
					*/
					continue;
				}
				break;
			}
			add_string_to_strv(&new_envp, orig_env_var);
		}
	} else {
		/* copy environment. Some things will be broken with
		 * this qemu (for example, prelinking won't work, etc) */
		int i;
		for (i = 0; i < new_envp.strv_num_orig_elems; i++) {
			switch (orig_env[i][0]) {
			case 'G':
			case 'N':
			case 'L':
				/* drop GCONV_PATH, NLSPATH and LOCPATH
				 * completely */ 
				if (matches_gconv_path_nlspath_or_locpath(orig_env[i])) {
					continue;
				}
			}
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: envp: add '%s'", __func__, orig_env[i]);
			add_string_to_strv(&new_envp, orig_env[i]);
		}
	}
	/* This was already handled in the loops, above:
	 *		hack_envp = { }
	 *		for i = 1, #new_envp do
	 *			if string.match(new_envp[i], "^GCONV_PATH=.*") or
	 *			   string.match(new_envp[i], "^NLSPATH=.*") or
	 *			   string.match(new_envp[i], "^LOCPATH=.*") then
	 *				-- skip
	 *			else
	 *				table.insert(hack_envp, new_envp[i])
	 *			end
	 *		end
	 *		new_envp, hack_envp = hack_envp, nil
	*/
	
	/*
	 *		-- libsb2 will replace LD_PRELOAD and LD_LIBRARY_PATH
	 *		-- env.vars, we don't need to worry about what the
	 *		-- application will see in those - BUT we need
	 *		-- to set those variables for Qemu itself.
	 *		-- Fortunately that is easy: 
	 *		local qemu_ldlibpath
	 *		local qemu_ldpreload
	 *		if conf_cputransparency.qemu_ld_library_path == "" then
	 *			qemu_ldlibpath = "LD_LIBRARY_PATH=" .. host_ld_library_path
	 *		else
	 *			qemu_ldlibpath = conf_cputransparency.qemu_ld_library_path
	 *		end
	 *		if conf_cputransparency.qemu_ld_preload == "" then
	 *			qemu_ldpreload = "LD_PRELOAD=" ..  host_ld_preload
	 *		else
	 *			qemu_ldpreload = conf_cputransparency.qemu_ld_preload
	 *		end
	 *
	 *		table.insert(new_envp, qemu_ldlibpath)
	 *		table.insert(new_envp, qemu_ldpreload)
	*/
	{
		const char	*qemu_ldlibpath = NULL;
		const char	*qemu_ldpreload = NULL;
		char	*cp = NULL;

		/* LD_LIBRARY_PATH */
		namev_in_ruletree[2] = "qemu_ld_library_path";
		qemu_ldlibpath = get_cputransp_string(namev_in_ruletree);
		if (!qemu_ldlibpath || (*qemu_ldlibpath == '\0')) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: No qemu_ld_library_path, using host's ld_library_path (%s)",
				__func__, conf_cputransparency_name);
			qemu_ldlibpath = ruletree_catalog_get_string("config", "host_ld_library_path");
			assert(asprintf(&cp, "LD_LIBRARY_PATH=%s", qemu_ldlibpath) > 0);	
		} else {
			/* qemu_ldlibpath has LD_LIBRARY_PATH= prefix */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: set ld_library_path (%s) = %s",
				__func__, conf_cputransparency_name, qemu_ldlibpath);
			cp = strdup(qemu_ldlibpath);
		} 
		add_string_to_strv(&new_envp, cp);

		/* LD_PRELOAD */
		namev_in_ruletree[2] = "qemu_ld_preload";
		qemu_ldpreload = get_cputransp_string(namev_in_ruletree);
		if (!qemu_ldpreload || (*qemu_ldpreload == '\0')) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: No qemu_ld_preload, using host's ld_preload (%s)",
				__func__, conf_cputransparency_name);
			qemu_ldpreload = ruletree_catalog_get_string("config", "host_ld_preload");
			assert(asprintf(&cp, "LD_PRELOAD=%s", qemu_ldpreload) > 0);	
		} else {
			/* qemu_ldpreload has LD_PRELOAD= prefix */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: set ld_preload (%s) = %s",
				__func__, conf_cputransparency_name, qemu_ldpreload);
			cp = strdup(qemu_ldpreload);
		} 
		add_string_to_strv(&new_envp, cp);
	}

	/*		-- unmapped file is exec'd
	 *		table.insert(new_argv, filename)
	*/
	add_string_to_strv(&new_argv, *filename);

	/*
	 *		--
	 *		-- Append arguments for target process (skip argv[0]
	 *		-- as this is done using -0 switch).
	 *		--
	 *		for i = 2, #argv do
	 *			table.insert(new_argv, argv[i])
	 *		end
	*/
	{
		/* Append arguments for target process (skip argv[0],
		 * either it has been already added by -0 switch,
		 * or can't be added at all)
		*/
		int i;
		for (i = 1; i < new_argv.strv_num_orig_elems; i++) {
			add_string_to_strv(&new_argv, new_argv.strv_orig_v[i]);
		}
	}

 	/*
	 *		-- environment&args were changed
	 *		return 0, new_filename, filename, #new_argv, new_argv,
	 *			#new_envp, new_envp
	 *
	 * FIXME: Not implemented:
	 *	elseif conf_cputransparency.method_is_sbrsh then
	 *		return sb_execve_postprocess_sbrsh(exec_policy,
	 *    			exec_type, mapped_file, filename, argv, envp)
	 *	end
	 *
	 *	-- no changes
	 *	return 1, mapped_file, filename, #argv, argv, #envp, envp
	 *end
	*/

	*set_envp = new_envp.strv_new_v;
	*set_argv = new_argv.strv_new_v;
	*mapped_file = new_mapped_file;
	*filename = new_filename;
	/* instruct caller to always use argv,envp,filename
	 * and mapped file from this routine */
	return(0);
}

/* CPU transparency.
 *
 * Same result code as Lua returned, the return value:
 *    -1 = do not execute.
 *    0 = argc&argv were updated, OK to execute with the new params
 *    1 = ok to exec directly with orig.arguments
 *
 * ===== FIXME: This assumes CPU transparency method == Qemu always !!!
*/
int exec_postprocess_cpu_transparency_executable(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp,
	const char *conf_cputransparency_name)
{
	return(exec_postprocess_qemu(exec_policy_name, mapped_file,
		filename, binary_name, orig_argv, set_argv,
		orig_env, set_envp, conf_cputransparency_name));
}

/* Directly executed static host binaries (alternatively,
 * static binaries can be executed with Qemu. See sb_exec.c)
 *
 * Same result code as Lua returned, the return value:
 *    -1 = do not execute.
 *    0 = argc&argv were updated, OK to execute with the new params
 *    1 = ok to exec directly with orig.arguments
*/
int exec_postprocess_host_static_executable(
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
        const char **orig_argv,
        const char ***set_argv,
	const char **orig_env,
        const char ***set_envp)
{
	exec_policy_handle_t	eph;
	struct strv_s		new_envp;
	struct strv_s		new_argv;
	char			*cp;
	const char		 *hp;
	int			i;

	/* Preparations.
	 * no arguments can be added, but two additions to env:
	 * 1) LD_PRELOAD
	 * 2) LD_LIBRARY_PATH
	*/
	if (exec_postprocess_prepare(exec_policy_name, &eph, mapped_file,
		filename, binary_name, orig_argv,
		&new_argv, 0, orig_env, &new_envp, 2))
			return(-1);

	hp = ruletree_catalog_get_string("config", "host_ld_library_path");
	assert(asprintf(&cp, "LD_LIBRARY_PATH=%s", hp) > 0);
	add_string_to_strv(&new_envp, cp);
	
	hp = ruletree_catalog_get_string("config", "host_ld_preload");
	assert(asprintf(&cp, "LD_PRELOAD=%s", hp) > 0);
	add_string_to_strv(&new_envp, cp);

	/* Append arguments */
	for (i = 0; i < new_argv.strv_num_orig_elems; i++) {
		add_string_to_strv(&new_argv, new_argv.strv_orig_v[i]);
	}
	/* Append rest of env. */
	for (i = 0; i < new_envp.strv_num_orig_elems; i++) {
		add_string_to_strv(&new_envp, new_envp.strv_orig_v[i]);
	}

	*set_envp = new_envp.strv_new_v;
	*set_argv = new_argv.strv_new_v;
	/* instruct caller to always use argv,envp,filename
	 * and mapped file from this routine */
	return(0);
}
