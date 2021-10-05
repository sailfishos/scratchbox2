/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * License: LGPL-2.1
 */

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include "sb2_execs.h"

/* A very straightforward conversion from Lua:
 *
 * Same result code as Lua returned, the return value:
 *  0: argv / envp were modified; mapped_interpreter was set
 *  1: argv / envp were not modified; mapped_interpreter was set
 *  2: argv / envp were not modified; caller should call ordinary path 
 *      mapping to find the interpreter
 *  -1: deny exec.
*/
int exec_map_script_interpreter(
	exec_policy_handle_t	eph,
	const char *exec_policy_name,
	const char *interpreter,
	const char *interp_arg,
	const char *mapped_script_filename,
	const char *orig_script_filename,
	char	   **argv,
	const char **new_exec_policy_p,
	char	   **mapped_interpreter_p)
{
	const char		*log_level = NULL;
	uint32_t /*FIXME*/	rule_list_offs = 0;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: interp=%s, interp_arg=%s policy=%s ", __func__,
		interpreter, (interp_arg?interp_arg:""), exec_policy_name);

        assert(exec_policy_name != NULL);
        assert(mapped_interpreter_p != NULL);

	(void)mapped_script_filename; /* not used */

	log_level = EXEC_POLICY_GET_STRING(eph, script_log_level);
	if (log_level) {
		const char	*log_message;

		log_message = EXEC_POLICY_GET_STRING(eph, script_log_message);
		SB_LOG(sblog_level_name_to_number(log_level), "%s", log_message);
	}

	if (EXEC_POLICY_GET_BOOLEAN(eph, script_deny_exec)) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: denied, returns -1", __func__);
		return(-1);
	}

	rule_list_offs = EXEC_POLICY_GET_RULES(eph, script_interpreter_rules);
	if (rule_list_offs) {
		char *mapping_result = NULL;
		const char *new_exec_policy = NULL;

		SB_LOG(SB_LOGLEVEL_DEBUG, "Applying exec_policy '%s' to script", exec_policy_name);

		mapping_result = custom_map_abstract_path(rule_list_offs,
			orig_script_filename/*binary_name*/,
			interpreter, "map_script_interpreter",
			SB2_INTERFACE_CLASS_EXEC,
			&new_exec_policy);

		if (mapping_result) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: result path = '%s'", __func__, mapping_result);

			if (new_exec_policy_p) *new_exec_policy_p = new_exec_policy;
			*mapped_interpreter_p = mapping_result;

			if (EXEC_POLICY_GET_BOOLEAN(eph, script_set_argv0_to_mapped_interpreter)) {
				argv[0] = strdup(mapping_result);
				return(0);
			}
			return(1);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: no result path", __func__);
		}
	}
	/* The default case:
         * exec policy says nothing about the script interpreters.
         * use ordinary path mapping to find it */
	return(2);
}


