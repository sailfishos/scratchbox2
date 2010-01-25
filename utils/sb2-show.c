/* sb2-show:
 * SB2 Mapping rule testing utility
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#include <unistd.h>
#include <string.h>
#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

#include "exported.h"
#include "sb2.h"
#include "scratchbox2_version.h"

#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif
#ifdef HAVE__NSGETENVIRON
#define environ (*_NSGetEnviron())
#else
 #ifndef __GLIBC__
  extern char **environ;
 #endif
#endif

static void *libsb2_handle = NULL;

/* -------------------- wrappers functions for calling functions from
 * 			libsb2.so; if sb2-show is executed outside of
 *			an sb2 session, libsb2.so is not available.
*/

/* create a wrapper to a function returning void */
#define LIBSB2_VOID_CALLER(funct_name, param_list, param_names) \
	static void call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && libsb2_handle) \
			fnptr = dlsym(libsb2_handle, #funct_name); \
		if (fnptr) { \
			((void(*)param_list)fnptr)param_names; \
			return; \
		} \
	} \
	extern void funct_name param_list; /* ensure that we got the prototype right */

/* create a wrapper to a function with returns something */
#define LIBSB2_CALLER(return_type, funct_name, param_list, param_names, errorvalue) \
	static return_type call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && libsb2_handle) \
			fnptr = dlsym(libsb2_handle, #funct_name); \
		if (fnptr) { \
			return(((return_type(*)param_list)fnptr)param_names); \
		} \
		return(errorvalue); \
	} \
	extern return_type funct_name param_list; /* ensure that we got the prototype right */

/* create call_sb2show__binary_type__() */
LIBSB2_CALLER(char *, sb2show__binary_type__,
	(const char *filename), (filename),
	NULL)

/* create call_sb2show__map_path2__() */
LIBSB2_CALLER(char *, sb2show__map_path2__,
	(const char *binary_name, const char *mapping_mode,
	const char *fn_name, const char *pathname, int *readonly),
	(binary_name, mapping_mode, fn_name, pathname, readonly),
	NULL)

/* create call_sb2show__execve_mods__() */
LIBSB2_CALLER(int, sb2show__execve_mods__,
	(char *file, char *const *orig_argv, char *const *orig_envp,
	char **new_file, char ***new_argv, char ***new_envp),
	(file, orig_argv, orig_envp, new_file, new_argv, new_envp),
	-1)

/* create call_sb2__set_active_exec_policy_name__() */
LIBSB2_VOID_CALLER(sb2__set_active_exec_policy_name__,
	(const char *name), (name))

/* create call_sb2__load_and_execute_lua_file__() */
LIBSB2_VOID_CALLER(sb2__load_and_execute_lua_file__,
	(const char *filename), (filename))

/* create call_sb2__lua_c_interface_version__() */
LIBSB2_CALLER(const char *, sb2__lua_c_interface_version__,
	(void), (), NULL)

/* create call_sb2show__get_real_cwd__() */
LIBSB2_CALLER(char *, sb2show__get_real_cwd__,
	(const char *binary_name, const char *fn_name),
	(binary_name, fn_name),
	NULL)

/* create call_sblog_vprintf_line_to_logfile() */
LIBSB2_VOID_CALLER(sblog_vprintf_line_to_logfile,
	(const char *file, int line,
        int level, const char *format, va_list ap),
	(file, line, level, format, ap))

/* create call_sb2__read_string_variable_from_lua__() */
LIBSB2_CALLER(char *, sb2__read_string_variable_from_lua__,
	(const char *name), (name), NULL)

int sb_loglevel__ = SB_LOGLEVEL_uninitialized;

/* need to have a copy of sblog_printf_line_to_logfile() here;
 * variable arguments are not compatible with the LIBSB2_CALLER()
 * thing above. Must use va_lists instead.
*/
void sblog_printf_line_to_logfile(
	const char      *file,
	int             line,
	int             level,
	const char      *format,
	...)
{
	va_list ap;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) return;

	va_start(ap, format);
	call_sblog_vprintf_line_to_logfile(file, line, level, format, ap);
	va_end(ap);
}

/* -------------------- end of wrappers functions. */

static void usage_exit(const char *progname, const char *errmsg, int exitstatus)
{
	if (errmsg) 
		fprintf(stderr, "%s: Error: %s\n", progname, errmsg);

	fprintf(stderr, "\n%s: Usage:\n", progname);
	fprintf(stderr, "\t%s [options] command [parameters]\n", progname);

	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr,
	    "\t-b binary_name show using binary_name as name of\n"
	    "\t               the calling program\n"
	    "\t-p policy_name show using policy_name as name of\n"
	    "\t               the active exec policy\n"
	    "\t-f function    show using 'function' as callers name\n"
	    "\t-D             ignore directories while verifying path lists\n"
	    "\t-v             be more verbose\n"
	    "\t-t             report elapsed time (real time elapsed while\n"
	    "\t               executing 'command')\n"
	    "\t-x filename    load and execute Lua code from file before\n"
	    "\t               executing 'command'\n"
	    "\t-X filename    load and execute Lua code from file after\n"
	    "\t               executing 'command'\n"
	    "\t-g port        use port as qemu gdbserver listening port\n"
	    "\t               (default port is 1234)\n");

	fprintf(stderr, "Commands:\n");
	fprintf(stderr,
	    "\tpath [path1] [path2].. show mappings of pathnames\n"
	    "\twhich [path1] [path2].. (like the 'path' command, but less verbose)\n"
	    "\trealcwd                show real current working directory\n"
	    "\tpwd                    show virtual current working directory\n"
	    "\texec file [argv1] [argv2]..\n"
	    "\t                       show execve() modifications\n"
	    "\texec-cmdline file [argv1] [argv2]..\n"
	    "\t                       show execve() modifications on\n"
	    "\t                       a single line (does not show full\n"
	    "\t                       details)\n"
	    "\tqemu-debug-exec file argv0 [argv1] [argv2]..\n"
	    "\t                       show command line that can be used to\n"
	    "\t                       start target binary under qemu\n"
	    "\t                       gdbserver\n"
	    "\tlog-error 'message'    add an error message to the log\n"
	    "\tlog-warning 'message'  add a warning message to the log\n"
	    "\tbinarytype realpath    detect & show type of program at\n"
	    "\t                       'realpath' (already mapped path)\n"
	    "\tverify-pathlist-mappings required-prefix [ignorelist]\n"
	    "\t                       read list of paths from stdin and\n"
	    "\t                       check that all paths will be mapped to\n"
	    "\t                       required prefix\n"
	    "\tvar variablename       show value of a string variable\n"
	    "\texecluafile filename   load and execute Lua code from file\n"
	    "\t                       (useful for debugging and/or\n"
	    "\t                       benchmarking sb2 itself)\n"
	    "\tlibraryinterface       show preload library interface version\n"
	    "\t                       (the Lua <-> C code interface)\n");

	fprintf(stderr, "\n"
	    "'%s' must be executed inside sb2 sandbox (see the 'sb2'"
	    "command)\n", progname);

	exit(exitstatus);
}

static int command_show_variable(
	int verbose,
	const char *progname, 
	const char *varname)
{
	char *value = call_sb2__read_string_variable_from_lua__(varname);

	if (value) {
		if (verbose) printf("%s = \"%s\"\n", varname, value);
		else printf("%s\n", value);
		free(value);
		return(0);
	} 
	/* failed */
	if (verbose) printf("%s: %s does not exist\n",
		progname, varname);
	return(1);
}

static int elem_count(char *const *elems)
{
	int count = 0;
	char **p = (char **)elems;
	while (*p) {
		p++; count++;
	}
	return count;
}

static char **join_env_vecs(char **env1, char **env2)
{
	int     elemc1 = (env1 ? elem_count(env1) : 0);
	int     elemc2 = (env2 ? elem_count(env2) : 0);
	char    **p;
	int     i;
	char    **new_elems;

	new_elems = (char **)calloc(elemc1 + elemc2 + 1, sizeof(char *));
	for (i = 0, p = env1; p && *p; p++) {
		int insert_to_new_elems = 1;

		/* variables in "env2" override "env1": */
		if (env2) {
			char *cp = strchr(*p, '=');
			int namelen = 0;

			if (cp) namelen = cp - *p;
			if (namelen) {
				char **p2;
				for (p2 = env2; p2 && *p2; p2++) {
					if (!strncmp(*p,*p2,namelen+1)) {
						/* same variable found from
						 * env2, ignore the value from
						 * env1 */
						insert_to_new_elems = 0;							break;
					}
				}
			}
		}
		if (insert_to_new_elems)
			new_elems[i++] = strdup(*p);
	}
	for (p = env2; p && *p; p++) {
		new_elems[i++] = strdup(*p);
	}
	new_elems[i] = NULL;

	return(new_elems);
}

static int compar_strvec_elems(const void *e1, const void *e2)
{
	char **s1 = (char **)e1, **s2 = (char **)e2;

	return(strcmp(*s1,*s2));
}

static int diff_strvecs(char **orig_vec, char **new_vec,
	int verbose, int singleline)
{
	char **op = orig_vec;
	char **np = new_vec;
	int num_diffs = 0;
	char *sp = "";

	while (op && *op && np && *np) {
		int r = strcmp(*op,*np);
		if (r == 0) {
			if (verbose)
				printf("%15s: %s\n", "unmodified", *op);
			op++, np++;
		} else {
			const char *cp = strchr(*op,'=');
			int namelen = 0;

			if (cp) namelen = cp - *op;
			if (namelen) {
				if (!strncmp(*op,*np,namelen+1)) {
					/* same name: value was modified */
					if (singleline) {
						printf("%s%s", sp, *np);
						sp = " ";
					} else {
						printf("%15s: %s\n", "modified, old",
							 *op);
						printf("%15s: %s\n", "          new",
							 *np);
					}
					op++, np++;
					num_diffs++;
					continue;
				}
			}
			/* not modified. Something was added or removed. */
			if (r < 0) {
				if (!singleline) {
					printf("%15s: %s\n", "removed", *op);
				}
				op++;
				num_diffs++;
				continue;
			}
			if (singleline) {
				printf("%s%s", sp, *np);
				sp = " ";
			} else {
				printf("%15s: %s\n", "added", *np);
			}
			num_diffs++;
			np++;
		}
	}
	/* now either "op" or "np" or both have been processed*/
	if (!singleline) while (op && *op) {
		printf("%15s: %s\n", "Removed", *op);
		num_diffs++;
		op++;
	}
	while (np && *np) {
		if (!singleline) {
			printf("%15s: %s\n", "Added", *np);
		} else {
			printf("%s%s", sp, *np);
			sp = " ";
		}
		num_diffs++;
		np++;
	}
	return(num_diffs);
}

/* sort argv / envp */
static void sort_strvec(char **elems)
{
	qsort(elems, elem_count(elems), sizeof(char *), compar_strvec_elems);
}

typedef void (*print_exec_fn_t)(void *, const char *, const char *,
    int, char *[], char *[], char *[], int);

/*ARGSUSED*/
static void print_exec(void *priv,
    const char *file, const char *mapped_path,
    int readonly_flag, char *new_argv[], char *orig_env[],
    char *new_env[], int verbose)
{
	int i;

	(void)priv;

	printf("File\t%s\n", file);
	printf("Mapped\t%s%s\n", mapped_path,
	    (readonly_flag ? " (readonly)" : ""));

	for (i = 0; new_argv[i]; i++) {
		printf("argv[%d]\t'%s'\n", i, new_argv[i]);
	}

	/* compare orig. and new envs. a very simple diff. */
	sort_strvec(orig_env);
	sort_strvec(new_env);

	printf("Environment:\n");
	if (diff_strvecs(orig_env, new_env, verbose, 0/*singleline*/) == 0) {
		printf(" (no changes)\n");
	}
}

/* Print exec parameters on a single line,
 * used for command "exec-cmdline". Note that this
 * will not print argv[0] at all or env.vars that 
 * were cleared. Only the "exec" command show all
 * information.
*/
/*ARGSUSED*/
static void print_exec_cmdline(void *priv,
    const char *file, const char *mapped_path,
    int readonly_flag, char *new_argv[], char *orig_env[],
    char *new_env[], int verbose)
{
	int i;

	(void)priv;
	(void)file;
	(void)readonly_flag;
	(void)verbose;

	/* First, print modified or added environment variables: */

	/* compare orig. and new envs. a very simple diff. */
	sort_strvec(orig_env);
	sort_strvec(new_env);

	if (diff_strvecs(orig_env, new_env, 0/*verbose*/, 1/*singleline*/)) {
		printf(" ");
	}

	/* next, print the mapped file name + argv[1..(argc-1)] */
	printf("%s", mapped_path);

	for (i = 1; new_argv[i]; i++) {
		printf(" %s", new_argv[i]);
	}
	printf("\n");
}

/*ARGSUSED*/
static void print_qemu_debug_exec(void *priv,
    const char *file, const char *mapped_path,
    int readonly_flag, char *new_argv[], char *orig_env[],
    char *new_env[], int verbose)
{
	const char *debug_port = (const char *)priv;
	int i;

	(void)file;
	(void)readonly_flag;
	(void)orig_env;
	(void)new_env;
	(void)verbose;

	printf("%s ", mapped_path);
	printf("-g %s", debug_port);
	/* drop argv[0] */
	for (i = 1; new_argv[i]; i++) {
		printf(" %s", new_argv[i]);
	}
	printf("\n");
}

static void command_show_exec(
	const char *binary_name,
	const char *fn_name,
	const char *progname, 
	int argc, char **argv,
	char **additional_env,
	int verbose,
	print_exec_fn_t print_exec_fn,
	void *priv)
{
	char	*new_file;
	char	**new_argv;
	char	**new_envp;
	char	**orig_env0;
	char	**orig_env;
	char	*mapped_path = NULL;
	int	readonly_flag;
	char	*ba[2];
	
	if (argc < 1) {
		usage_exit(progname, "Too few parameters for this command", 1);
	}

	/* fix __SB2_BINARYNAME in the environment that is going to be
	 * given to sb2show__execve_mods__() (otherwise it would be "sb2-show"
	*/
	if (asprintf(&ba[0], "__SB2_BINARYNAME=%s", binary_name) < 0) {
		printf("Fatal: asprintf failed");
		exit(1);
	}

	ba[1] = NULL;
	orig_env0 = join_env_vecs(environ, ba);
	/* add user-specified environment varaibles */
	orig_env = join_env_vecs(orig_env0, additional_env);

	if (call_sb2show__execve_mods__(argv[0], argv, orig_env,
		&new_file, &new_argv, &new_envp) < 0) {
		printf("Exec denied (%s)\n", strerror(errno));
	} else {
		/* do_exec() will map the path just after argvenvp
		 * modifications have been done, do that here also
		*/
		mapped_path = call_sb2show__map_path2__(binary_name,
		    "", fn_name, new_file, &readonly_flag);

		if (!mapped_path) {
			printf("Mapping failed (%s)\n", strerror(errno));
		} else {
			/* call the actual print function */
			(*print_exec_fn)(priv, new_file, mapped_path,
			    readonly_flag, new_argv, orig_env, new_envp, verbose);
		}
	}
}

static void command_show_path(const char *binary_name, const char *fn_name,
	int show_destination_only, char **argv)
{
	char	*mapped_path = NULL;
	int	readonly_flag;

	while (*argv) {
		mapped_path = call_sb2show__map_path2__(binary_name, "", 
			fn_name, *argv, &readonly_flag);
		if (!mapped_path) {
			printf("%s: Mapping failed\n", *argv);
		} else if (show_destination_only) {
			printf("%s\n", mapped_path);
		} else {
			printf("%s => %s%s\n", 
				*argv, mapped_path,
				(readonly_flag ? " (readonly)" : ""));
		}
		argv++;
	}
}

static void command_show_binarytype(const char *binary_name,
        const char *fn_name, int verbose, char **argv)
{
	char	*mapped_path = NULL;
	int	readonly_flag;

	while (*argv) {
		/* sb2show__binary_type__() operates on
		 * real paths; map the path first.. */
		mapped_path = call_sb2show__map_path2__(binary_name, "",
			fn_name, *argv, &readonly_flag);
		if (!mapped_path) {
			printf("%s: Mapping failed\n", *argv);
		} else {
			char *type = call_sb2show__binary_type__(mapped_path);
			if (verbose) {
				printf("%s: %s\n", mapped_path, type);
			} else {
				printf("%s\n", type);
			}
			free(type);
		}
		argv++;
	}
}

static void command_show_realcwd(const char *binary_name, const char *fn_name)
{
	char	*real_cwd_path = NULL;

	real_cwd_path = call_sb2show__get_real_cwd__(binary_name, fn_name);
	printf("%s\n", real_cwd_path ? real_cwd_path : "<null>");
}

static void command_show_pwd(const char *progname)
{
	char	path_buf[PATH_MAX + 1];

	if (getcwd(path_buf, sizeof(path_buf))) {
		printf("%s\n", path_buf);
	} else {
		if (errno == ERANGE) {
			fprintf(stderr, "%s: CWD is longer than PATH_MAX\n",
				progname);
		} else {
			perror(progname);
		}
		exit(1);
	}
}

/* read paths from stdin, report paths that are not mapped to specified
 * directory.
 * returns 0 if all OK, 1 if one or more paths were not mapped.
*/
static int command_verify_pathlist_mappings(
	const char *binary_name,
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
		int	require_both = 0;
		/* 1 == ignore, 2 == require_both */
		int	compare_mode = 1;

		if ((len > 0) && (path_buf[len-1] == '\n')) {
			path_buf[--len] = '\0';
		}
		if (len == 0) continue;

		for (ignore_path = argv+1; *ignore_path; ignore_path++) {
			int	ign_len;

			if (**ignore_path == '@') {
				if (!strcmp(*ignore_path, "@ignore:")) {
					compare_mode = 1;
					continue;
				}
				if (!strcmp(*ignore_path, "@require-both:")) {
					compare_mode = 2;
					continue;
				}
			}

			if (compare_mode == 1) {
				ign_len = strlen(*ignore_path);

				if (!strncmp(path_buf, *ignore_path, ign_len)) {
					ignore_this = 1;
					if (verbose)
						printf("IGNORED by prefix: %s\n",
							path_buf);
					break;
				}
			} else {
				/* FIXME: check it is 2 */
				ign_len = strlen(*ignore_path);

				if (!strncmp(path_buf, *ignore_path, ign_len)) {
					require_both = 1;
					if (verbose)
						printf("REQUIRE_BOTH by prefix: %s\n",
							path_buf);
					break;
				}
			}
		}

		if (ignore_this) continue;

		mapped_path = call_sb2show__map_path2__(binary_name, "",
				fn_name, path_buf, &readonly_flag);
		if (!mapped_path) {
			if (verbose)
				printf("%s: Mapping failed\n", path_buf);
			continue;
		}

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
			if (require_both) {
				result |= 2;
				if (verbose)
					printf("%s => %s%s: NOT OK (Require both)\n",
						path_buf, mapped_path,
						(readonly_flag ? " (readonly)" : ""));
			} else {
				result |= 1;
				if (verbose)
					printf("%s => %s%s: NOT OK\n",
						path_buf, mapped_path,
						(readonly_flag ? " (readonly)" : ""));
			}
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

static void command_show_libraryinterface(void)
{
	printf("%s\n", call_sb2__lua_c_interface_version__());
}

int main(int argc, char *argv[])
{
	int	opt;
	char	*progname = argv[0];
	char	*function_name = "ANYFUNCTION";
	char	*binary_name = "ANYBINARY";
	int	ignore_directories = 0;
	int	verbose = 0;
	int	report_time = 0;
	struct timeval	start_time, stop_time;
	int	ret = 0;
	char	*pre_cmd_file = NULL;
	char	*post_cmd_file = NULL;
	char	**additional_env = NULL;
	char	*debug_port = "1234";
	
	while ((opt = getopt(argc, argv, "hm:f:b:Dvtg:x:X:E:p:")) != -1) {
		switch (opt) {
		case 'h': usage_exit(progname, NULL, 0); break;
		case 'm':
			fprintf(stderr,
				 "%s: Warning: option -m is obsolete\n",
				argv[0]);
			break;
		case 'f': function_name = optarg; break;
		case 'b': binary_name = optarg; break;
		case 'p': call_sb2__set_active_exec_policy_name__(optarg); break;
		case 'D': ignore_directories = 1; break;
		case 'v': verbose = 1; break;
		case 't': report_time = 1; break;
		case 'g': debug_port = optarg; break;
		case 'x': pre_cmd_file = optarg; break;
		case 'X': post_cmd_file = optarg; break;
		case 'E':
			if (!strchr(optarg,'=')) {
				fprintf(stderr,
					 "%s: Error: parameter error in -E\n",
					argv[0]);
			} else if (!additional_env) {
				additional_env = calloc(2, sizeof(char*));
				additional_env[0] = strdup(optarg);
			} else {
				int n_elem = elem_count(additional_env);
				additional_env = realloc(additional_env,
					(n_elem+2)*sizeof(char*));
				additional_env[n_elem] = strdup(optarg);
				additional_env[n_elem+1] = NULL;
			}
			break;
		default: usage_exit(progname, "Illegal option", 1); break;
		}
	}

	/* disable mapping; dlopen must run without mapping. */
	setenv("SBOX_DISABLE_MAPPING", "1", 1/*overwrite*/);
	libsb2_handle = dlopen(LIBSB2_SONAME, RTLD_NOW);
	if (libsb2_handle) {

		if (verbose) {
			printf("%s: libsb2 FOUND\n", progname);
		}
		int *loglevelptr = NULL;
		loglevelptr = dlsym(libsb2_handle, "sb_loglevel__");
		if (loglevelptr) {
			sb_loglevel__ = *loglevelptr;
		} else {
			sb_loglevel__ = 0;
		}
	} else {
		if (verbose) {
			printf("%s: libsb2 was not found\n", progname);
		}
		sb_loglevel__ = 0;
	}
	/* enable mapping */
	unsetenv("SBOX_DISABLE_MAPPING");

	if (!libsb2_handle) 
		usage_exit(progname, "This command can only be used "
			"inside a session (e.g. 'sb2 sb2-show ...')", 1);

	/* check parameters */
	if (optind >= argc) 
		usage_exit(progname, "Wrong number of parameters", 1);

	/* Execute the "pre-command" file before starting the clock (if both
	 * -x and -t options were used)
	*/
	if (pre_cmd_file)
		call_sb2__load_and_execute_lua_file__(pre_cmd_file);

	if (report_time) {
		if (gettimeofday(&start_time, (struct timezone *)NULL) < 0) {
			fprintf(stderr, "%s: Failed to get time\n", progname);
			report_time = 0;
		}
	}

	/* params ok, go and perform the action */
	if (!strcmp(argv[optind], "libraryinterface")) {
		command_show_libraryinterface();
	} else if (!strcmp(argv[optind], "path")) {
		command_show_path(binary_name, function_name, 
			0/*verbose output*/, argv + optind + 1);
	} else if (!strcmp(argv[optind], "which")) {
		command_show_path(binary_name, function_name, 
			1/*show only dest.path*/, argv + optind + 1);
	} else if (!strcmp(argv[optind], "realcwd")) {
		command_show_realcwd(binary_name, function_name);
	} else if (!strcmp(argv[optind], "pwd")) {
		command_show_pwd(progname);
	} else if (!strcmp(argv[optind], "exec")) {
		command_show_exec(binary_name, function_name,
			progname, argc - (optind+1), argv + optind + 1,
			additional_env, verbose,
			print_exec, NULL);
	} else if (!strcmp(argv[optind], "exec-cmdline")) {
		command_show_exec(binary_name, function_name,
			progname, argc - (optind+1), argv + optind + 1,
			additional_env, 0/*verbose*/,
			print_exec_cmdline, NULL);
	} else if (!strcmp(argv[optind], "binarytype")) {
		command_show_binarytype(binary_name, function_name,
			verbose, argv + optind + 1);
	} else if (!strcmp(argv[optind], "qemu-debug-exec")) {
		command_show_exec(binary_name, function_name,
			progname, argc - (optind+1), argv + optind + 1,
			additional_env, verbose,
			print_qemu_debug_exec, debug_port);
	} else if (!strcmp(argv[optind], "log-error")) {
		command_log(argv + optind + 1, SB_LOGLEVEL_ERROR);
	} else if (!strcmp(argv[optind], "log-warning")) {
		command_log(argv + optind + 1, SB_LOGLEVEL_WARNING);
	} else if (!strcmp(argv[optind], "verify-pathlist-mappings")) {
		ret = command_verify_pathlist_mappings(binary_name,
			function_name, ignore_directories,
			verbose, progname, argv + optind + 1);
	} else if (!strcmp(argv[optind], "var")) {
		ret = command_show_variable(verbose, progname, argv[optind+1]);
	} else if (!strcmp(argv[optind], "execluafile")) {
		call_sb2__load_and_execute_lua_file__(argv[optind+1]);
	} else {
		usage_exit(progname, "Unknown command", 1);
	}

	if (report_time) {
		if (gettimeofday(&stop_time, (struct timezone *)NULL) < 0) {
			fprintf(stderr, "%s: Failed to get time\n", progname);
		} else {
			long secs;
			long usecs;

			secs = stop_time.tv_sec - start_time.tv_sec;
			usecs = stop_time.tv_usec - start_time.tv_usec;
			if (usecs < 0) {
				usecs += 1000000;
				secs--;
			}
			printf("TIME: %ld.%06ld\n", secs, usecs);
		}
	}

	/* Execute the "prost-command" file after the clock has been stopped
	 * (if both -X and -t options were used)
	*/
	if (post_cmd_file)
		call_sb2__load_and_execute_lua_file__(post_cmd_file);

	return(ret);
}

