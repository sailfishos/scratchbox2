/* sb2-show:
 * SB2 Mapping rule testingutility
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#include <unistd.h>
#include <config.h>
#include <config_hardcoded.h>

#include "libsb2.h"
#include "exported.h"

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
		"\nCommands:\n"
		"\tpath [path1] [path2].."
			"\tShow mappings of pathnames\n"
		"\texec file argv0 [argv1] [argv2].."
			"\tShow execve() modifications\n"
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
	mapped_path = sb2show__map_path__(binary_name, mapping_mode, fn_name,
		 new_file);
	printf("Mapped\t%s\n", mapped_path);

	for (i = 0; new_argv[i]; i++) {
		printf("argv[%d]\t%s\n", i, new_argv[i]);
	}
}

static void command_show_path(const char *binary_name,
	const char *mapping_mode,
	const char *fn_name,  char **argv)
{
	char	*mapped_path = NULL;

	while (*argv) {
		mapped_path = sb2show__map_path__(binary_name, mapping_mode, 
			fn_name, *argv);
		printf("%s => %s\n", *argv, mapped_path);
		argv++;
	}
}

int main(int argc, char *argv[])
{
	int	opt;
	char	*progname = argv[0];
	char	*mapping_mode = NULL;
	char	*function_name = "ANYFUNCTION";
	char	*binary_name = "ANYBINARY";
	
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

	while ((opt = getopt(argc, argv, "hm:f:b:")) != -1) {
		switch (opt) {
		case 'h': usage_exit(progname, NULL, 0); break;
		case 'm': mapping_mode = optarg; break;
		case 'f': function_name = optarg; break;
		case 'b': binary_name = optarg; break;
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
	} else {
		usage_exit(progname, "Unknown command", 1);
	}

	return(0);
}

