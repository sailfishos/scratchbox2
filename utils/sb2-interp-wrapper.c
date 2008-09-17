/*
 * Copyright (C) 2008 Movial
 * Authors: Timo Savola <tsavola@movial.fi>
 *
 * The purpose of this program is to wrap a shell (or any) script invocation so
 * that the open(script) done by the interpreter won't appear at a standard
 * binary directory for the redirector.  By passing a symlink located in /tmp
 * to the interpreter, the 'install' mapping rules do the right thing.
 */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <unistd.h>
#include <config.h>
#include <config_hardcoded.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "exported.h"

#define PROGNAME	"sb2-interp-wrapper"

enum interp_type {
	INTERP_SHELL,
};

static int (*sb_execvep)(const char *file, char *const argv[], char *const envp[]);
static char **origenv;
static char linkdir[] = "/tmp/sb2-interp-wrapper-XXXXXX";
static char *linkpath;

static void cleanup(void)
{
	unlink(linkpath);
	rmdir(linkdir);
}

static int startswith(const char *subject, const char *prefix)
{
	return strstr(subject, prefix) == subject;
}

static int shortopt(const char *subject, char opt, const char *modes)
{
	const char *mode;

	if (strlen(subject) < 2)
		return 0;

	for (mode = modes; 1; mode++) {
		if (*mode == '\0')
			return 0;

		if (subject[0] == *mode) {
			if (subject[1] == *mode)
				return 0;

			break;
		}
	}

	return strchr(subject + 1, opt) != NULL;
}

static void exec(char **argv)
{
	sb_execvep(argv[0], argv, origenv);

	fprintf(stderr, "%s: %s: %s\n", PROGNAME, argv[0], strerror(errno));
	exit(1);
}

int main(int argc, char **argv)
{
	int envlen;
	const char *interpname;
	enum interp_type type;
	int script;
	const char *scriptname;
	int signum;
	pid_t pid;
	int status;

	sb_execvep = dlsym(NULL, "sb_execvep");
	if (sb_execvep == NULL) {
		fprintf(stderr, "%s (%s): unable to look up sb_execvep symbol "
		        "- internal scratchbox error\n", PROGNAME, argv[0]);
		return 1;
	}

	for (envlen = 0; environ[envlen]; envlen++)
		;

	origenv = malloc((envlen + 1) * sizeof (char *));
	if (origenv == NULL) {
		perror(NULL);
		return 1;
	}

	memcpy(origenv, environ, (envlen + 1) * sizeof (char *));

	putenv("SBOX_DISABLE_MAPPING=1");

	interpname = basename(argv[0]);  /* GNU basename */

	if (strcmp(interpname, "sh") == 0 || strcmp(interpname, "bash") == 0) {
		type = INTERP_SHELL;
	} else if (strcmp(interpname, PROGNAME) == 0) {
		fprintf(stderr, "don't call %s manually\n", PROGNAME);
		return 1;
	} else {
		fprintf(stderr, "%s: unknown interpreter: %s\n", PROGNAME,
		        argv[0]);
		exec(argv);
	}

	for (script = 1; script < argc; script++) {
		const char *arg = argv[script];

		switch (type) {
		case INTERP_SHELL:
			/* commands are not read from a file? */
			if (shortopt(arg, 'c', "-") ||
			    shortopt(arg, 's', "-") ||
			    shortopt(arg, 'i', "-"))
				exec(argv);

			/* skip option parameters */
			if (shortopt(arg, 'o', "-+") ||
			    shortopt(arg, 'O', "-+") ||
			    startswith(arg, "--init-file") ||
			    startswith(arg, "--rcfile")) {
				script++;
				continue;
			}

			break;
		}

		if (strcmp(arg, "--") == 0) {
			script++;
			break;
		}

		if (arg[0] != '-')
			break;
	}

	if (script >= argc)
		exec(argv);

	if (!(startswith(argv[script], "/bin/") ||
	      startswith(argv[script], "/usr/bin/") ||
	      startswith(argv[script], "/usr/local/bin/")))
		exec(argv);

	if (mkdtemp(linkdir) == NULL) {
		perror(NULL);
		return 1;
	}

	scriptname = basename(argv[script]);  /* GNU basename */

	linkpath = malloc(strlen(linkdir) + 1 + strlen(scriptname) + 1);
	if (linkpath == NULL) {
		perror(NULL);
		return 1;
	}

	strcpy(linkpath, linkdir);
	strcat(linkpath, "/");
	strcat(linkpath, scriptname);

	if (getenv("SBOX_MAPPING_DEBUG"))
		fprintf(stderr, "%s (%s): \"%s\" -> \"%s\"\n", PROGNAME,
		        argv[0], argv[script], linkpath);

	if (symlink(argv[script], linkpath) < 0) {
		perror(linkpath);
		return 1;
	}

	atexit(cleanup);

	for (signum = 1; signum < 32; signum++)
		switch (signum) {
		case SIGCHLD:
		case SIGCONT:
		case SIGSTOP:
		case SIGURG:
		case SIGWINCH:
			break;

		default:
			signal(signum, exit);
			break;
		}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	}

	if (pid == 0) {
		char *tools;
		char *interp;

		tools = sb2__read_string_variable_from_lua__("sbox_tools_root");
		if (tools && tools[0] != '\0' && strcmp(tools, "/") != 0) {
			interp = malloc(strlen(tools) + strlen(argv[0]) + 1);
			if (interp == NULL) {
				perror(NULL);
				return 1;
			}

			strcpy(interp, tools);
			strcat(interp, argv[0]);

			argv[0] = interp;
		}

		argv[script] = linkpath;

		exec(argv);
	}

	if (waitpid(pid, &status, 0) < 0) {
		perror("waitpid");
		return 1;
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	fprintf(stderr, "%s exited with status %d\n", argv[0], status);
	return 1;
}
