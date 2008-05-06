/* sb2-show:
 * SB2 Mapping rule testing utility
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#include <unistd.h>
#include <config.h>
#include <config_hardcoded.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "exported.h"
#include "sb2.h"

static void usage_exit(const char *progname, const char *errmsg, int exitstatus)
{
	if (errmsg) 
		fprintf(stderr, "%s: Error: %s\n", progname, errmsg);

	fprintf(stderr, 
		"\n%s: Usage:\n"
		"\t%s [options] command [parameters]\n"
		"\nOptions:\n"
		"\t-b binary_name\tshow using binary_name as name of "
			"the calling program\n"
		"\t-m mode\t\tshow using named mapping mode "
			"(default=current mode)\n"
		"\t-f function\tshow using 'function' as callers name\n"
		"\t-D\tignore directories while verifying path lists\n"
		"\t-v\tverbose.\n"
		"\nCommands:\n"
		"\tpath [path1] [path2].."
			"\tShow mappings of pathnames\n"
		"\texec file argv0 [argv1] [argv2].."
			"\tShow execve() modifications\n"
		"\tlog-error 'message'"
			"\tAdd an error message to the log\n"
		"\tlog-warning 'message'"
			"\tAdd a warning message to the log\n"
		"\tverify-pathlist-mappings required-prefix [ignorelist]"
			"\tread list of paths from stdin and/n"
			"\t\tcheck that all paths will be mapped to required prefix/n"
		"\n'%s' must be executed inside sb2 sandbox"
			" (see the 'sb2' command)\n",
		progname, progname, progname);

	exit(exitstatus);
}

static void command_show_exec(
	const char *binary_name,
	const char *mapping_mode,
	const char *fn_name,
	const char *progname, 
	int argc, char **argv)
{
	char	*new_file;
	char	**new_argv;
	char	**new_envp;
	char	*mapped_path = NULL;
	int	readonly_flag;
	int	i;
	
	if (argc < 2) {
		usage_exit(progname, "Too few parameters for this command", 1);
	}
	sb2show__execve_mods__(argv[0], argv, environ,
		&new_file, &new_argv, &new_envp);
	printf("File\t%s\n", new_file);

	/* do_exec() will map the path just after argvenvp modifications
	 * have been done, do that here also
	*/
	mapped_path = sb2show__map_path2__(binary_name, mapping_mode, fn_name,
		 new_file, &readonly_flag);
	printf("Mapped\t%s%s\n", mapped_path, (readonly_flag ? " (readonly)" : ""));

	for (i = 0; new_argv[i]; i++) {
		printf("argv[%d]\t%s\n", i, new_argv[i]);
	}
}

static void command_show_path(const char *binary_name,
	const char *mapping_mode,
	const char *fn_name,  char **argv)
{
	char	*mapped_path = NULL;
	int	readonly_flag;

	while (*argv) {
		mapped_path = sb2show__map_path2__(binary_name, mapping_mode, 
			fn_name, *argv, &readonly_flag);
		printf("%s => %s%s\n", 
			*argv, mapped_path,
			(readonly_flag ? " (readonly)" : ""));
		argv++;
	}
}

/* read paths from stdin, report paths that are not mapped to specified
 * directory.
 * returns 0 if all OK, 1 if one or more paths were not mapped.
*/
static int command_verify_pathlist_mappings(
	const char *binary_name,
	const char *mapping_mode,
	const char *fn_name,
	int ignore_directories,
	int verbose,
	const char *progname,
	char **argv)
{
	int	destination_prefix_len;
	char	path_buf[PATH_MAX + 1];
	const char *required_destination_prefix = argv[0];
	int	result = 0;

	if (!required_destination_prefix) {
		usage_exit(progname, "'destination_prefix' is missing", 1);
	}
	destination_prefix_len = strlen(required_destination_prefix);

	while (fgets(path_buf, sizeof(path_buf), stdin)) {
		int len = strlen(path_buf);
		char	*mapped_path = NULL;
		int	readonly_flag;
		int	destination_prefix_cmp_result;
		char	**ignore_path;
		int	ignore_this = 0;

		if ((len > 0) && (path_buf[len-1] == '\n')) {
			path_buf[--len] = '\0';
		}
		if (len == 0) continue;

		for (ignore_path = argv+1; *ignore_path; ignore_path++) {
			int	ign_len = strlen(*ignore_path);

			if (!strncmp(path_buf, *ignore_path, ign_len)) {
				ignore_this = 1;
				if (verbose)
					printf("IGNORED by prefix: %s\n",
						path_buf);
				break;
			}
		}

		if (ignore_this) continue;

		mapped_path = sb2show__map_path2__(binary_name, mapping_mode,
				fn_name, path_buf, &readonly_flag);

		if (ignore_directories) {
			struct stat statbuf;

			if ((stat(mapped_path, &statbuf) == 0) &&
			   S_ISDIR(statbuf.st_mode)) {
				if (verbose)
					printf("%s => %s: dir, ignored\n",
						path_buf, mapped_path);
				continue;
			}
		}

		destination_prefix_cmp_result = strncmp(mapped_path,
			required_destination_prefix, destination_prefix_len);
		if (destination_prefix_cmp_result) {
			result = 1;
			printf("%s => %s%s: NOT OK\n",
				path_buf, mapped_path,
				(readonly_flag ? " (readonly)" : ""));
		} else {
			/* mapped OK. */
			if (verbose)
				printf("%s => %s%s: Ok\n",
					path_buf, mapped_path,
					(readonly_flag ? " (readonly)" : ""));
		}
	}
	return (result);
}

static void command_log(char **argv, int loglevel)
{
	SB_LOG(loglevel, "%s", argv[0]);
}

int main(int argc, char *argv[])
{
	int	opt;
	char	*progname = argv[0];
	char	*mapping_mode = NULL;
	char	*function_name = "ANYFUNCTION";
	char	*binary_name = "ANYBINARY";
	int	ignore_directories = 0;
	int	verbose = 0;
	
#if 0 || defined(enable_this_after_sb2_preload_library_startup_has_been_fixed)
	/* FIXME: this should be able to check if we are inside the sb2
	 * sandbox, but that is not currently possible (instead initialization
	 * of the preload library will fail even before main() is entered.
	 * this happens because the preload library was not designed to
	 * be used as ordinary library, which is exactly what we are doing 
	 * now... initialization code of the library should be modified.
	*/

	/* check that we are running inside 'sb2' environment */
	if (!getenv("SBOX_LUA_SCRIPTS") ||
	   !getenv("SBOX_MAPMODE")) {
		usage_exit(progname, "Not inside scratchboxed environment", 1);
	}
#endif

	while ((opt = getopt(argc, argv, "hm:f:b:Dv")) != -1) {
		switch (opt) {
		case 'h': usage_exit(progname, NULL, 0); break;
		case 'm': mapping_mode = optarg; break;
		case 'f': function_name = optarg; break;
		case 'b': binary_name = optarg; break;
		case 'D': ignore_directories = 1; break;
		case 'v': verbose = 1; break;
		default: usage_exit(progname, "Illegal option", 1); break;
		}
	}

	/* check parameters */
	if (optind >= argc) 
		usage_exit(progname, "Wrong number of parameters", 1);
	if (!mapping_mode) 
		mapping_mode = getenv("SBOX_MAPMODE");

	/* params ok, go and perform the action */
	if (!strcmp(argv[optind], "path")) {
		command_show_path(binary_name, mapping_mode, function_name, 
			argv + optind + 1);
	} else if (!strcmp(argv[optind], "exec")) {
		command_show_exec(binary_name, mapping_mode, function_name,
			progname, argc - (optind+1), argv + optind + 1);
	} else if (!strcmp(argv[optind], "log-error")) {
		command_log(argv + optind + 1, SB_LOGLEVEL_ERROR);
	} else if (!strcmp(argv[optind], "log-warning")) {
		command_log(argv + optind + 1, SB_LOGLEVEL_WARNING);
	} else if (!strcmp(argv[optind], "verify-pathlist-mappings")) {
		return command_verify_pathlist_mappings(binary_name,
			mapping_mode, function_name, ignore_directories,
			verbose, progname, argv + optind + 1);
	} else {
		usage_exit(progname, "Unknown command", 1);
	}

	return(0);
}

