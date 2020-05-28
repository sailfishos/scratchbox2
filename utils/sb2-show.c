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

void *libsb2_handle = NULL;

/* command line options which will be exposed to sub-commands */
typedef struct cmdline_options_s {
	const	char	*progname;	/* well, not really an option. */
	int		opt_verbose;
	int		opt_ignore_directories;
	const char	*binary_name;
	const char	*function_name; 
	int		function_name_set; 
	const char	*debug_port; 

	char	**additional_env; /* there can be multiple -E options */
} cmdline_options_t;

typedef struct command_table_s {
	const char	*cmd_name;
	int		cmd_must_have_session;
	int		cmd_min_argc;
	int		cmd_max_argc;
	int		(*cmd_fn)(const struct command_table_s *cmd_s,
				const cmdline_options_t *cmd_options,
				int cmd_argc, char *cmd_argv[]);
	const char	*cmd_helptext;
} command_table_t;

/* -------------------- wrapper functions for calling functions from
 * 			libsb2.so; if sb2-show is executed outside of
 *			an sb2 session, libsb2.so is not available.
*/

#include "libsb2callers.h"

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

/* create call_sb2show__reverse_path__() */
LIBSB2_CALLER(char *, sb2show__reverse_path__,
	(const char *func_name, \
        const char *abs_path, uint32_t classmask),
	(func_name, abs_path, classmask),
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

/* create call_sb2show__map_network_addr__() */
LIBSB2_CALLER(int, sb2show__map_network_addr__,
	(const char *binary_name, const char *fn_name,
	 const char *protocol, const char *addr_type,
	 const char *dst_addr, int port,
	 char **addr_bufp, int *new_portp),
	(binary_name, fn_name, protocol, addr_type,
	 dst_addr, port, addr_bufp, new_portp), -1)

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

static void usage_exit(const char *progname, const char *errmsg, int exitstatus,
	const command_table_t *cmds)
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
	    "\t-g port        use port as qemu gdbserver listening port\n"
	    "\t               (default port is 1234)\n");

	if (cmds) {
		fprintf(stderr, "Commands:\n");
		while (cmds->cmd_name) {
			fprintf(stderr, "%s\n", cmds->cmd_helptext);
			if (!cmds->cmd_must_have_session) {
				fprintf(stderr, 
				  "\t                       "
				  "(can be used without a session)\n");
			}
			cmds++;
		}
	}

	fprintf(stderr, "\n"
	    "'%s' must be executed inside sb2 sandbox (see the 'sb2' "
	    "command)\n", progname);

	exit(exitstatus);
}

static int cmd_var(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	char *value = call_sb2__read_string_variable_from_lua__(cmd_argv[1]);

	(void)cmdp;
	(void)cmd_argc;
	if (value) {
		if (opts->opt_verbose) printf("%s = \"%s\"\n", cmd_argv[1], value);
		else printf("%s\n", value);
		free(value);
		return(0);
	} 
	/* failed */
	if (opts->opt_verbose) printf("%s: %s does not exist\n",
		opts->progname, cmd_argv[1]);
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

/* print arg, quote it if empty or contains whitespace */
static void q_print_arg(const char *prefix, const char *arg)
{
	int q = 0;

	if (!arg) arg = "";
	if (!*arg) {
		q = 1;
	} else if (strchr(arg, ' ') || strchr(arg, '\t') || strchr(arg,'\n')) {
		q = 1;
	}

	if(q) {
		printf("%s'%s'", prefix, arg);
	} else {
		printf("%s%s", prefix, arg);
	}
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
				q_print_arg(sp, *np);
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
			q_print_arg(sp, *np);
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
		q_print_arg(" ", new_argv[i]);
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

static int cmd_net(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	if (!strcmp(cmd_argv[1], "addr")) {
		int res;
		char *new_addr;
		int  new_port;

		if (cmd_argc < 5) {
			usage_exit(opts->progname, "Too few parameters for subcommand 'net'", 1, NULL);
		}
		res = call_sb2show__map_network_addr__(opts->binary_name,
			opts->function_name, ""/*protocol. currently unused.*/,
			cmd_argv[2]/*addr_type*/, cmd_argv[3]/*dst_addr*/,
			atoi(cmd_argv[4])/*port*/, &new_addr, &new_port);

		printf("Result = %d, address = %s port = %d\n", 
			res, new_addr, new_port);
	} else {
		usage_exit(opts->progname, "Subcommand must be 'addr'", 1, NULL);
	}
	return(0);
}

static void command_show_exec(
	const cmdline_options_t *opts,
	int argc, char **argv,
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
	const char *binary_name = opts->binary_name;
	const char *fn_name = opts->function_name;
	const char *progname = opts->progname;
	char **additional_env = opts->additional_env;

	if (!opts->function_name_set) {
		/* function name was not specified by an option.
		 * default to execvp(); some modes have rules which
		 * depend on function name, and output may not be
		 * correct if mapping is called with default 
		 * "anyfunction" tag.
		*/
		fn_name = "execvp";
	}

	if (argc < 1) {
		usage_exit(progname, "Too few parameters for this command", 1, NULL);
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

static int cmd_reverse(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	cmd_argv++;
	while (*cmd_argv) {
		char *reversed_path;
		/* sb2show__binary_type__() operates on
		 * real paths; map the path first.. */
		reversed_path = call_sb2show__reverse_path__(
			opts->function_name, *cmd_argv, 0/*FIXME-should be classmask, but not used currently*/);
		if (!reversed_path) {
			printf("%s: Reversing failed\n", *cmd_argv);
		} else {
			printf("%s => %s\n", *cmd_argv, reversed_path);
			free(reversed_path);
		}
		cmd_argv++;
	}
	return(0);
}

static int cmd_binarytype(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	char	*mapped_path = NULL;
	int	readonly_flag;

	(void)cmdp;
	(void)cmd_argc;
	cmd_argv++;
	while (*cmd_argv) {
		/* sb2show__binary_type__() operates on
		 * real paths; map the path first.. */
		mapped_path = call_sb2show__map_path2__(opts->binary_name, "",
			opts->function_name, *cmd_argv, &readonly_flag);
		if (!mapped_path) {
			printf("%s: Mapping failed\n", *cmd_argv);
		} else {
			char *type = call_sb2show__binary_type__(mapped_path);
			if (opts->opt_verbose) {
				printf("%s: %s\n", mapped_path, type);
			} else {
				printf("%s\n", type);
			}
			free(type);
		}
		cmd_argv++;
	}
	return(0);
}

static int cmd_realcwd(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	char	*real_cwd_path = NULL;

	(void)cmdp;
	(void)cmd_argc;
	(void)cmd_argv;
	real_cwd_path = call_sb2show__get_real_cwd__(opts->binary_name, opts->function_name);
	printf("%s\n", real_cwd_path ? real_cwd_path : "<null>");
	return(0);
}

static int cmd_realpath(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	char	real_path[PATH_MAX+1];
	char	*rp;

	(void)cmdp;
	(void)cmd_argc;
	(void)opts;
	rp = realpath(cmd_argv[1], real_path);
	if (rp) {
		printf("%s\n", rp);
	} else {
		perror(opts->progname);
	}
	return(0);
}

static int cmd_pwd(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	char	path_buf[PATH_MAX + 1];

	(void)cmdp;
	(void)cmd_argc;
	(void)cmd_argv;
	if (getcwd(path_buf, sizeof(path_buf))) {
		printf("%s\n", path_buf);
	} else {
		if (errno == ERANGE) {
			fprintf(stderr, "%s: CWD is longer than PATH_MAX\n",
				opts->progname);
		} else {
			perror(opts->progname);
		}
		return(1);
	}
	return(0);
}

/* read paths from stdin, report paths that are not mapped to specified
 * directory.
 * returns 0 if all OK, 1 if one or more paths were not mapped.
*/
static int cmd_verify_pathlist_mappings(const command_table_t *cmdp,
			const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	int	destination_prefix_len;
	char	path_buf[PATH_MAX + 1];
	const char *required_destination_prefix = cmd_argv[1];
	int	result = 0;

	(void)cmdp;
	(void)cmd_argc;
	if (!required_destination_prefix) {
		usage_exit(opts->progname, "'destination_prefix' is missing", 1, NULL);
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

		for (ignore_path = cmd_argv+2; *ignore_path; ignore_path++) {
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
					if (opts->opt_verbose)
						printf("IGNORED by prefix: %s\n",
							path_buf);
					break;
				}
			} else {
				/* FIXME: check it is 2 */
				ign_len = strlen(*ignore_path);

				if (!strncmp(path_buf, *ignore_path, ign_len)) {
					require_both = 1;
					if (opts->opt_verbose)
						printf("REQUIRE_BOTH by prefix: %s\n",
							path_buf);
					break;
				}
			}
		}

		if (ignore_this) continue;

		mapped_path = call_sb2show__map_path2__(opts->binary_name, "",
				opts->function_name, path_buf, &readonly_flag);
		if (!mapped_path) {
			if (opts->opt_verbose)
				printf("%s: Mapping failed\n", path_buf);
			continue;
		}

		if (opts->opt_ignore_directories) {
			struct stat64 statbuf;

			if ((stat64(mapped_path, &statbuf) == 0) &&
			   S_ISDIR(statbuf.st_mode)) {
				if (opts->opt_verbose)
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
				if (opts->opt_verbose)
					printf("%s => %s%s: NOT OK (Require both)\n",
						path_buf, mapped_path,
						(readonly_flag ? " (readonly)" : ""));
			} else {
				result |= 1;
				if (opts->opt_verbose)
					printf("%s => %s%s: NOT OK\n",
						path_buf, mapped_path,
						(readonly_flag ? " (readonly)" : ""));
			}
		} else {
			/* mapped OK. */
			if (opts->opt_verbose)
				printf("%s => %s%s: Ok\n",
					path_buf, mapped_path,
					(readonly_flag ? " (readonly)" : ""));
		}
	}
	return (result);
}

static int cmd_log_error(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	(void)opts;
	SB_LOG(SB_LOGLEVEL_ERROR, "%s", cmd_argv[1]);
	return(0);
}

static int cmd_log_warning(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	(void)opts;
	SB_LOG(SB_LOGLEVEL_WARNING, "%s", cmd_argv[1]);
	return(0);
}

static int cmd_libraryinterface(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	(void)cmd_argv;
	(void)opts;
	printf("%s\n", call_sb2__lua_c_interface_version__());
	return(0);
}

static int cmd_path(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	command_show_path(opts->binary_name, opts->function_name, 
		0/*verbose output*/, cmd_argv + 1);
	return(0);
}

static int cmd_which(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	command_show_path(opts->binary_name, opts->function_name, 
		1/*show only dest.path*/, cmd_argv + 1);
	return(0);
}

static int cmd_exec(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	command_show_exec(opts,
			cmd_argc - 1, cmd_argv + 1,
			opts->opt_verbose,
			print_exec, NULL);
	return(0);
}

static int cmd_exec_cmdline(const command_table_t *cmdp, const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	command_show_exec(opts,
			cmd_argc - 1, cmd_argv + 1,
			0/*verbose*/,
			print_exec_cmdline, NULL);
	return(0);
}

static int cmd_qemu_debug_exec(const command_table_t *cmdp,
			const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	command_show_exec(opts,
			cmd_argc - 1, cmd_argv + 1,
			opts->opt_verbose,
			print_qemu_debug_exec, (void*)opts->debug_port);
	return(0);
}

static int cmd_acct_on(const command_table_t *cmdp,
			const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	(void)opts;
	if (acct(cmd_argv[1]) < 0) {
		perror(opts->progname);
		return(1);
	}
	return(0);
}

static int cmd_acct_off(const command_table_t *cmdp,
			const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	(void)cmdp;
	(void)cmd_argc;
	(void)opts;
	(void)cmd_argv;
	if (acct(NULL) < 0) {
		perror(opts->progname);
		return(1);
	}
	return(0);
}

static int cmd_start(const command_table_t *cmdp,
			const cmdline_options_t *opts,
			int cmd_argc, char *cmd_argv[])
{
	int	i;

	(void)cmdp;
	(void)opts;
	if (opts->opt_verbose) {
		for (i = 0; i < cmd_argc-1; i++)
			printf("%d: '%s'\n", i, cmd_argv[i+1]);
	}
	execvp(cmd_argv[1], cmd_argv+1);
	fprintf(stderr,"%s could not be executed: %s\n",cmd_argv[1], strerror(errno));
	return(1);
}

const command_table_t commands[] = {
	/* name	 must_have_session	min_argc,max_argc, fn
	 * helptext */
	{ "acct_on",	0,		2,	2,	cmd_acct_on,
	  "\tacct_on file           activate process accounting, see man acct(2)\n"
	  "\t                       (requires superuser rights)"},
	{ "acct_off",	0,		1,	1,	cmd_acct_off,
	  "\tacct_off               deactivate process accounting"},
	{ "binarytype",	1,		1,	9999,	cmd_binarytype,
	  "\tbinarytype realpath    detect & show type of program at\n"
	  "\t                       'realpath' (already mapped path)"},
	{ "exec",	1,		1,	9999,	cmd_exec,
	  "\texec file [argv1] [argv2]..\n"
	  "\t                       show execve() modifications"},
	{ "exec-cmdline", 1,		1,	9999,	cmd_exec_cmdline,
	    "\texec-cmdline file [argv1] [argv2]..\n"
	    "\t                       show execve() modifications on\n"
	    "\t                       a single line (does not show full\n"
	    "\t                       details)"},
	{ "libraryinterface",1,		1,	1,	cmd_libraryinterface,
	  "\tlibraryinterface       show preload library interface version\n"
	  "\t                       (the Lua <-> C code interface)"},
	{ "log-error",	1,		2,	2,	cmd_log_error,
	  "\tlog-error 'message'    add an error message to the log"},
	{ "log-warning",	1,		2,	2,	cmd_log_warning,
	  "\tlog-warning 'message'  add a warning message to the log"},
	{ "net",	1,		2,	9999,	cmd_net,
	  "\tnet subcmd [argvs]..\n"
	  "\t                       show networking info"},
	{ "path", 	1,		1,	9999,	cmd_path,
	  "\tpath [path1] [path2].. show mappings of pathnames"},
	{ "pwd", 	1,		1,	1,	cmd_pwd,
	  "\tpwd                    show virtual current working directory"},
	{ "qemu-debug-exec", 1,		1,	9999,	cmd_qemu_debug_exec,
	    "\tqemu-debug-exec file argv0 [argv1] [argv2]..\n"
	    "\t                       show command line that can be used to\n"
	    "\t                       start target binary under qemu\n"
	    "\t                       gdbserver"},
	{ "realcwd", 	1,		1,	1,	cmd_realcwd,
	  "\trealcwd                show real current working directory"},
	{ "realpath", 	0,		2,	2,	cmd_realpath,
	  "\trealpath path          call realpath(path) and print the result"},
	{ "reverse", 	1,		2,	9999,	cmd_reverse,
	  "\treverse path [path2]   reverse-map path(s) and print the results"},
	{ "start", 	1,		2,	9999,	cmd_start,
	  "\tstart command [params] Execute 'command' (this is used internally\n"
	  "\t                       during session startup)"},
	{ "var",	1,		2,	2,	cmd_var,
	  "\tvar variablename       show value of a string variable"},
	{ "verify-pathlist-mappings",1,	2,	9999,	cmd_verify_pathlist_mappings,
	  "\tverify-pathlist-mappings required-prefix [ignorelist]\n"
	  "\t                       read list of paths from stdin and\n"
	  "\t                       check that all paths will be mapped to\n"
	  "\t                       required prefix"},
	{ "which", 	1,		1,	9999,	cmd_which,
	  "\twhich [path1] [path2].. (like the 'path' command, but less verbose)"},
	{ NULL, 0, 0, 0, NULL, NULL } /* End of command table */
};

int main(int argc, char *argv[])
{
	int	opt;
	int	report_time = 0;
	struct timeval	start_time, stop_time;
	int	ret = 0;
	char	*active_exec_policy_name = NULL;
	cmdline_options_t	opts;
	const	command_table_t	*cmdp;
	int	subcmd_argc = 0;
	char	**subcmd_argv = NULL;
	
	memset(&opts, 0, sizeof(opts));
	opts.progname = argv[0];
	opts.function_name = "ANYFUNCTION";
	opts.function_name_set = 0;
	opts.binary_name = "ANYBINARY";
	opts.debug_port = "1234";

	while ((opt = getopt(argc, argv, "hm:f:b:Dvtg:x:X:E:p:")) != -1) {
		switch (opt) {
		case 'h': usage_exit(opts.progname, NULL, 0, commands); break;
		case 'm':
			fprintf(stderr,
				 "%s: Warning: option -m is obsolete\n",
				argv[0]);
			break;
		case 'f': opts.function_name = optarg;
			  opts.function_name_set = 1; break;
		case 'b': opts.binary_name = optarg; break;
		case 'p': active_exec_policy_name = optarg; break;
		case 'D': opts.opt_ignore_directories = 1; break;
		case 'v': opts.opt_verbose = 1; break;
		case 't': report_time = 1; break;
		case 'g': opts.debug_port = optarg; break;
#if 0
		/* -x and -X were used previously, but were removed
		 * when Lua was removed from libsb2. Think twice
		 * before recycling these options.
		*/
		case 'x': pre_cmd_file = optarg; break;
		case 'X': post_cmd_file = optarg; break;
#endif
		case 'E':
			if (!strchr(optarg,'=')) {
				fprintf(stderr,
					 "%s: Error: parameter error in -E\n",
					argv[0]);
			} else if (!opts.additional_env) {
				opts.additional_env = calloc(2, sizeof(char*));
				opts.additional_env[0] = strdup(optarg);
			} else {
				int n_elem = elem_count(opts.additional_env);
				opts.additional_env = realloc(opts.additional_env,
					(n_elem+2)*sizeof(char*));
				opts.additional_env[n_elem] = strdup(optarg);
				opts.additional_env[n_elem+1] = NULL;
			}
			break;
		default: usage_exit(opts.progname, "Illegal option", 1, commands); break;
		}
	}

	/* check parameters */
	if (optind >= argc) 
		usage_exit(opts.progname, "No command", 1, commands);

	/* find the command. */
	cmdp = commands;
	while (cmdp->cmd_name) {
		if (!strcmp(argv[optind], cmdp->cmd_name)) {
			subcmd_argc = argc - (optind);
			subcmd_argv = argv + optind;

			if (subcmd_argc < cmdp->cmd_min_argc) {
				usage_exit(opts.progname,
					"Not enough parameters", 1, commands);
			}
			if (subcmd_argc > cmdp->cmd_max_argc) {
				usage_exit(opts.progname,
					"Too many parameters", 1, commands);
			}
			break;
		}
		cmdp++;
	}
	if (!cmdp->cmd_name) {
		usage_exit(opts.progname, "Unknown command", 1, commands);
	}
	/* cmdp is valid now */

	if (cmdp->cmd_must_have_session) {
		/* disable mapping; dlopen must run without mapping. */
		setenv("SBOX_DISABLE_MAPPING", "1", 1/*overwrite*/);
		libsb2_handle = dlopen(LIBSB2_SONAME, RTLD_NOW);
		if (libsb2_handle) {

			if (opts.opt_verbose) {
				printf("%s: libsb2 FOUND\n", opts.progname);
			}
			int *loglevelptr = NULL;
			loglevelptr = dlsym(libsb2_handle, "sb_loglevel__");
			if (loglevelptr) {
				sb_loglevel__ = *loglevelptr;
			} else {
				sb_loglevel__ = 0;
			}
		} else {
			if (opts.opt_verbose) {
				printf("%s: libsb2 was not found\n", opts.progname);
			}
			sb_loglevel__ = 0;
		}
		/* enable mapping */
		unsetenv("SBOX_DISABLE_MAPPING");

		if (!libsb2_handle) 
			usage_exit(opts.progname, "This command can only be used "
				"inside a session (e.g. 'sb2 sb2-show ...')", 1, commands);
	}

	if (libsb2_handle && active_exec_policy_name) {
		call_sb2__set_active_exec_policy_name__(active_exec_policy_name);
	}

	if (report_time) {
		if (gettimeofday(&start_time, (struct timezone *)NULL) < 0) {
			fprintf(stderr, "%s: Failed to get time\n", opts.progname);
			report_time = 0;
		}
	}

	/* execute the command */
	ret = cmdp->cmd_fn(cmdp, &opts, subcmd_argc, subcmd_argv);

	if (report_time) {
		if (gettimeofday(&stop_time, (struct timezone *)NULL) < 0) {
			fprintf(stderr, "%s: Failed to get time\n", opts.progname);
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

	return(ret);
}

