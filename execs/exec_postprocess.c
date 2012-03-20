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

static int setenv_native_app_ld_preload(
	exec_policy_handle_t	eph,
	const char		**orig_env,
	const char		**new_env,
	int			new_env_idx)
{
	const char *new_path = NULL;
	const char *native_app_ld_preload = NULL;

	native_app_ld_preload = EXEC_POLICY_GET_STRING(eph, native_app_ld_preload);
	if (native_app_ld_preload) {
		new_path = native_app_ld_preload;
	} else {
		const char *native_app_ld_preload_prefix = NULL;
		const char *native_app_ld_preload_suffix = NULL;
		const char *libpath = get_users_ld_preload(orig_env);

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
	new_env[new_env_idx++] = new_path;

	return(new_env_idx);
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
static int setenv_native_app_ld_library_path(
	exec_policy_handle_t	eph,
	const char		**orig_env,
	const char		**new_env,
	int			new_env_idx)
{
	const char *new_path = NULL;
	const char *native_app_ld_library_path = NULL;

	native_app_ld_library_path = EXEC_POLICY_GET_STRING(eph, native_app_ld_library_path);
	if (native_app_ld_library_path) {
		new_path = native_app_ld_library_path;
	} else {
		const char *native_app_ld_library_path_prefix = NULL;
		const char *native_app_ld_library_path_suffix = NULL;
		const char *libpath = get_users_ld_library_path(orig_env);

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
	new_env[new_env_idx++] = new_path;

	return(new_env_idx);
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
	const char		**new_env = NULL;
	int			num_orig_env_vars = 0;
	int			new_env_max_size = 0;
	int			new_env_idx = 0;
	const char		**new_argv = NULL;
	int			num_orig_argv_vars = 0;
	int			new_argv_max_size = 0;
	int			new_argv_idx = 0;
	const char		*native_app_ld_so = NULL;
	char			*new_filename = *filename;
	char			*new_mapped_file = *mapped_file;
	int			first_argv_element_to_copy = 0;

	/* "generic part" (compare with Lua:sb_execve_postprocess() */
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: mapped_file=%s filename=%s binary_name=%s exec_policy_name=%s",
		__func__, *mapped_file, *filename, binary_name, exec_policy_name);

	eph = find_exec_policy_handle(exec_policy_name);
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
		"%s: Applying exec_policy '%s' to native binary",
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
	num_orig_env_vars = elem_count(orig_env);
	new_env_max_size = num_orig_env_vars + 6;
	/* add one to size for the terminating NULL */
	new_env = calloc(new_env_max_size + 1, sizeof(char*));

	{
		char	*cp;

		assert(asprintf(&cp, "__SB2_EXEC_POLICY_NAME=%s", exec_policy_name) > 0);
		new_env[new_env_idx++] = cp;
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
	num_orig_argv_vars = elem_count(orig_argv);
	new_argv_max_size = num_orig_argv_vars + 6;
	/* add one to 'nmemb' for the terminating NULL */
	new_argv = calloc(new_argv_max_size + 1, sizeof(char*));

	/* --- end of "generic part" */


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

		new_argv[new_argv_idx++] = native_app_ld_so;
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
			new_argv[new_argv_idx++] = "--rpath-prefix";
			new_argv[new_argv_idx++] = native_app_ld_so_rpath_prefix;
		} else {
			new_argv[new_argv_idx++] = "--inhibit-rpath";
			new_argv[new_argv_idx++] = "";
		}

		/* Lua:
		 *	if (exec_policy.native_app_ld_so_supports_nodefaultdirs) then
		 *		table.insert(new_argv, "--nodefaultdirs")
		 *	end
		*/
		if (EXEC_POLICY_GET_BOOLEAN(eph, native_app_ld_so_supports_nodefaultdirs)) {
			new_argv[new_argv_idx++] = "--nodefaultdirs";
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
			new_argv[new_argv_idx++] = "--argv0";
			new_argv[new_argv_idx++] = orig_argv[0];
		}
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: argv: add '%s'", __func__, *mapped_file);
		new_argv[new_argv_idx++] = *mapped_file;
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
	new_env_idx = setenv_native_app_ld_library_path(eph,
		orig_env, new_env, new_env_idx);
	new_env_idx = setenv_native_app_ld_preload(eph,
		orig_env, new_env, new_env_idx);

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
			new_env[new_env_idx++] = cp;
			assert(asprintf(&cp, "NLSPATH=%s", native_app_locale_path) > 0);
			new_env[new_env_idx++] = cp;
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
			new_env[new_env_idx++] = cp;
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
			new_env[new_env_idx++] = orig_env[i];
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
			new_argv[new_argv_idx++] = orig_argv[i];
		}
	}

	*set_envp = new_env;
	*set_argv = new_argv;
	assert(new_env_idx <= new_env_max_size);
	assert(new_argv_idx <= new_argv_max_size);
	*mapped_file = new_mapped_file;
	*filename = new_filename;
	/* instruct caller to always use argv,envp,filename
	 * and mapped file from this routine */
	return(0);
}

