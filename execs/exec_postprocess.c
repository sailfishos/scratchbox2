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
	const char **orig_env,
	struct strv_s	*new_envp)
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
	 * reserve space for new entries:
	 *  1) __SB2_EXEC_POLICY_NAME
	 *  2) LD_LIBRARY_PATH
	 *  3) LD_PRELOAD
	 *  4) LOCPATH (optional)
	 *  5) NLSPATH (optional)
	 *  6) GCONV_PATH (optional)
	*/
	init_strv(new_envp, "envp", orig_env, 6);
	{
		char	*cp;

		assert(asprintf(&cp, "__SB2_EXEC_POLICY_NAME=%s", exec_policy_name) > 0);
		add_string_to_strv(new_envp, cp);
	}

	/* allocate new argv.
	 * reserve space for new entries, needed if indirect startup:
	 *  1) ld.so path
	 *  2) "--rpath-prefix" or  "--inhibit-rpath"
	 *  3) exec_policy.native_app_ld_so_rpath_prefix or ""
	 *  4) "--nodefaultdirs" (optional)
	 *  5) "--argv0" (optional)
	 *  6) value for argv0 (optional)
	*/
	init_strv(new_argv, "argv", orig_argv, 6);

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

	if (exec_postprocess_prepare(exec_policy_name, &eph, mapped_file,
		filename, binary_name, orig_argv, &new_argv,
		orig_env, &new_envp)) return(1);

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

