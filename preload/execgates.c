/*
 * execgates -- exec*() GATEs for the scratchbox2 preload library
 * 		(also contains GATEs for the setrlimit() functions)
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 *	Lauri T. Aarnio
*/

/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include "libsb2.h"
#include "exported.h"

/* strchrnul(): Find the first occurrence of C in S or the final NUL byte.
 * This is not present on all systems, so we'll use our own version in sb2.
*/
static const char *sb2_strchrnul (const char *s, int c_in)
{
	const char	*ptr = strchr(s, c_in);

	if(!ptr) {
		/* this is exactly how strchrnul() performs if c_in was not
		 * found (see the manual page on modern Linuxes...): */
		ptr = s + strlen(s);
	}
	return(ptr);
}

/* setrlimit() and setrlimit64():
 * There is a bug in Linux/glibc's ld.so interaction: ld.so segfaults 
 * when it tries to  execute programs "manually" when stack limit
 * has been set to infinity. We need to observe setrlimit() and change
 * the stack limit back before exec... 
*/
static int restore_stack_before_exec = 0; /* 0, 1 or 64 */
static struct rlimit stack_limits_for_exec;
#ifndef __APPLE__
static struct rlimit64 stack_limits64_for_exec;
#endif
static int (*next_execve) (const char *filename, char *const argv [],
			char *const envp[]) = NULL;

int sb_next_execve(const char *file, char *const *argv, char *const *envp)
{
	if (next_execve == NULL) {
		next_execve = sbox_find_next_symbol(1, "execve");
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		char *buf = strvec_to_string(argv);

		if (buf) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "EXEC: file:%s argv:%s", file, buf);
			free(buf);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"EXEC: %s (failed to print argv)", file);
		}
	}
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE)) {
		char *buf = strvec_to_string(envp);

		if (buf) {
			SB_LOG(SB_LOGLEVEL_NOISE, "EXEC/env: %s", buf);
			free(buf);
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE,
				"EXEC: (failed to print env)");
		}
	}

	switch (restore_stack_before_exec) {
	case 1:
		SB_LOG(SB_LOGLEVEL_DEBUG, "EXEC: need to restore stack limit");

		if (setrlimit(RLIMIT_STACK, &stack_limits_for_exec) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"setrlimit(stack) failed, "
				"failed to restore limits before exec");
		}
		break;
#ifndef __APPLE__
	case 64:
		SB_LOG(SB_LOGLEVEL_DEBUG, "EXEC: need to restore stack limit");

		if (setrlimit64(RLIMIT_STACK, &stack_limits64_for_exec) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"setrlimit64(stack) failed, "
				"failed to restore limits before exec");
		}
		break;
#endif
	}

	return next_execve(file, argv, envp);
}


/* collect exec arguments from a varargs list to an array.
 * returns an allocated array (use free() to free it if exec fails)
*/
static char **va_exec_args_to_argv(
	const char *realfnname, 
	const char *arg0, 
	va_list args,
	char ***envpp)	/* execlp needs to get envp, it is after the NULL.. */
{
	char *next_arg;
	char **argv = NULL;
	int  n_elem;	/* number of elements in argv array, including the
			 * final NULL pointer */

	/* first we'll need a small array for arg0 and a NULL: */
	n_elem = 2;
	argv = malloc (n_elem * sizeof(char*));
	argv[0] = (char*)arg0;
	argv[1] = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE, "%s/varargs: 0=%s", realfnname, arg0);

	/* if there are any additional arguments, add them to argv
	 * calling realloc() every time (depending on what king of allocator
	 * is in use this might or might not be very efficient, but this 
	 * strategy keeps the code simple AND we can be sure that the 
	 * reallocation really works, unlike if this would reallocate 
	 * only after every 1024 elements or so... and after all, this 
	 * is used for exec-class functions, so this won't be executed 
	 * too often anyway => efficiency is probably not our primary concern)
	*/
	next_arg = va_arg (args, char *);
	while(next_arg) {
		n_elem++;
		argv = realloc (argv, n_elem * sizeof(char*));
		argv[n_elem - 2] = next_arg;
		argv[n_elem - 1] = NULL;

		SB_LOG(SB_LOGLEVEL_NOISE, "%s/varargs: %d=%s", 
			realfnname, n_elem-2, next_arg);

		next_arg = va_arg (args, char *);
	}
	/* next_arg==NULL now, get an optional envp if requested: */	
	if(envpp) {
		*envpp = va_arg (args, char **);
	}

	return(argv);
}


/* #include <unistd.h> */
int execl_gate(
	int (*real_execl_ptr)(const char *path, const char *arg, ...),
	const char *realfnname,
	const char *path,
	const char *arg,
	va_list args)
{
	char **argv;
	int ret;

	(void)real_execl_ptr; /* not used */

	argv = va_exec_args_to_argv(realfnname, arg, args, NULL);
	ret = execve_gate (NULL, realfnname, path, (char *const *) argv, 
		environ);
	free(argv);
	return(ret);
}


/* #include <unistd.h> */
int execle_gate(
	int (*real_execle_ptr)(const char *path, const char *arg, ...),
	const char *realfnname,
	const char *path,
	const char *arg,
	va_list args)
{
	char **argv;
	int ret;
	char **envp;

	(void)real_execle_ptr; /* not used */

	argv = va_exec_args_to_argv(realfnname, arg, args, &envp);
	ret = execve_gate (NULL, realfnname, path, (char *const *) argv, 
		(char *const *) envp);
	free(argv);
	return(ret);
}

/* Execute FILE, searching in the `PATH' environment variable if
   it contains no slashes, with all arguments after FILE until a
   NULL pointer and environment from `environ'.  */
int execlp_gate(
	int (*real_execlp_ptr)(const char *file, const char *arg, ...),
	const char *realfnname,
	const char *file,
	const char *arg,
	va_list args)
{
	char **argv;
	int ret;

	(void)real_execlp_ptr;	/* not used */

	argv = va_exec_args_to_argv(realfnname, arg, args, NULL);
	ret = execvp_gate (NULL, realfnname, file, (char *const *) argv);
	free(argv);
	return(ret);
}



/* #include <unistd.h> */
int execv_gate(
	int (*real_execv_ptr)(const char *path, char *const argv []),
	const char *realfnname,
	const char *path,
	char *const argv [])
{
	(void)real_execv_ptr;	/* not used */

	return execve_gate (NULL, realfnname, path, argv, environ);
}


/* #include <unistd.h> */
int execve_gate(
	int (*real_execve_ptr)(const char *filename, char *const argv [],
		char *const envp[]),
	const char *realfnname,
	const char *filename,
	char *const argv [],
	char *const envp[])
{
	(void)real_execve_ptr;
	return do_exec(realfnname, filename, argv, envp);
}

/* return a new argv[] */
static char **create_argv_for_script_exec(const char *file, char *const argv[])
{
	int	argc = 0;
	char	**new_argv = NULL;

	while (argv[argc++]);
	new_argv = (char**)calloc(argc+2, sizeof(char*));

	new_argv[0] = "/bin/sh";
	new_argv[1] = (char *) file;
	while (argc > 1) {
		new_argv[argc] = argv[argc - 1];
		argc--;
	}
	return(new_argv);
}


static int do_execvep(
	const char *realfnname,
	const char *file,
	char *const argv [],
	char *const envp [])
{
	if (*file == '\0') {
		/* We check the simple case first. */
		errno = ENOENT;
		return -1;
	}

	if (strchr (file, '/') != NULL) {
		/* Don't search when it contains a slash.  */
		execve_gate (NULL, realfnname, file, argv, envp);
		if (errno == ENOEXEC) {
			char **new_argv = create_argv_for_script_exec(
				file, argv);
			int err;
			execve_gate (NULL, realfnname, new_argv[0], new_argv, envp);
			err = errno;
			free(new_argv);
			errno = err;
		}
		return(-1);
	} else {
		int got_eacces = 0;
		const char *p;
		const char *path;
		char *name;
		size_t len;
		size_t pathlen;

		path = getenv ("PATH");
		if (path) path = strdup(path);
		if (path == NULL) {
			/* There is no `PATH' in the environment.
			   The default search path is the current directory
			   followed by the path `confstr' returns for `_CS_PATH'.  */
			char *new_path;
			len = confstr (_CS_PATH, (char *) NULL, 0);
			new_path = (char *) alloca (1 + len);
			new_path[0] = ':';
			(void) confstr (_CS_PATH, new_path + 1, len);
			path = new_path;
		}

		len = strlen (file) + 1;
		pathlen = strlen (path);
		name = alloca (pathlen + len + 1);
		/* Copy the file name at the top.  */
		name = (char *) memcpy (name + pathlen + 1, file, len);
		/* And add the slash.  */
		*--name = '/';

		p = path;
		do {
			char *startp;

			path = p;
			p = sb2_strchrnul (path, ':');

			if (p == path) {
				/* Two adjacent colons, or a colon at the beginning or the end
				   of `PATH' means to search the current directory.  */
				startp = name + 1;
			} else {
				startp = (char *) memcpy (name - (p - path), path, p - path);
			}

			/* Try to execute this name.  If it works, execv will not return.  */
			execve_gate (NULL, realfnname, startp, argv, envp);

			if (errno == ENOEXEC) {
				char **new_argv = create_argv_for_script_exec(
					startp, argv);
				int err;
				execve_gate (NULL, realfnname, new_argv[0], new_argv, envp);
				err = errno;
				free(new_argv);
				errno = err;
			}

			switch (errno) {
				case EACCES:
					/* Record the we got a `Permission denied' error.  If we end
					   up finding no executable we can use, we want to diagnose
					   that we did find one but were denied access.  */
					got_eacces = 1;
				case ENOENT:
				case ESTALE:
				case ENOTDIR:
					/* Those errors indicate the file is missing or not executable
					   by us, in which case we want to just try the next path
					   directory.  */
					break;

				default:
					/* Some other error means we found an executable file, but
					   something went wrong executing it; return the error to our
					   caller.  */
					return -1;
			}
		} while (*p++ != '\0');

		/* We tried every element and none of them worked.  */
		if (got_eacces)
			/* At least one failure was due to permissions, so report that
			   error.  */
			errno = EACCES;
	}

	/* Return the error from the last attempt (probably ENOENT).  */
	return -1;
}

int sb_execvep(
	const char *file,
	char *const argv [],
	char *const envp [])
{
	return do_execvep(__FUNCTION__, file, argv, envp);
}


/* #include <unistd.h> */
int execvp_gate(
	int (*real_execvp_ptr)(const char *file, char *const argv []),
	const char *realfnname,
	const char *file,
	char *const argv [])
{
	(void)real_execvp_ptr;	/* not used */
	return do_execvep(realfnname, file, argv, environ);
}


/* SETRLIMIT_ARG1_TYPE is defined in interface.master */

int setrlimit_gate(
	int (*real_setrlimit_ptr)(SETRLIMIT_ARG1_TYPE resource,
		const struct rlimit *rlp),
	const char *realfnname,
	SETRLIMIT_ARG1_TYPE resource,
	const struct rlimit *rlp)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: res=%d cur=%ld max=%ld",
		realfnname, resource, (long)rlp->rlim_cur, (long)rlp->rlim_max);

	if ((resource == RLIMIT_STACK) && (rlp->rlim_cur == RLIM_INFINITY)) {
		struct rlimit limit_now;

		if (getrlimit(RLIMIT_STACK, &limit_now) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: getrlimit(stack) failed", realfnname);
		} else {
			if (limit_now.rlim_cur != RLIM_INFINITY) {
				/* stack limit was not "unlimited", but we
				 * are going to set it to unlimited. */
				stack_limits_for_exec = limit_now;
				restore_stack_before_exec = 1;

				SB_LOG(SB_LOGLEVEL_NOTICE,
					"%s: Setting stack limit to infinity, "
					"old limit stored for next exec",
					realfnname);
			}
		}
	}
	return((*real_setrlimit_ptr)(resource,rlp));
}

#ifndef __APPLE__
int setrlimit64_gate(
	int (*real_setrlimit64_ptr)(SETRLIMIT_ARG1_TYPE resource,
		const struct rlimit64 *rlp),
	const char *realfnname,
	SETRLIMIT_ARG1_TYPE resource,
	const struct rlimit64 *rlp)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: res=%d cur=%ld max=%ld",
		realfnname, resource, (long)rlp->rlim_cur, (long)rlp->rlim_max);

	if ((resource == RLIMIT_STACK) && (rlp->rlim_cur == RLIM64_INFINITY)) {
		struct rlimit64 limit_now;

		if (getrlimit64(RLIMIT_STACK, &limit_now) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: getrlimit64(stack) failed", realfnname);
		} else {
			if (limit_now.rlim_cur != RLIM64_INFINITY) {
				/* stack limit was not "unlimited", but we
				 * are going to set it to unlimited. */
				stack_limits64_for_exec = limit_now;
				restore_stack_before_exec = 64;

				SB_LOG(SB_LOGLEVEL_NOTICE,
					"%s: Setting stack limit to infinity, "
					"old limit stored for next exec",
					realfnname);
			}
		}
	}
	return((*real_setrlimit64_ptr)(resource,rlp));
}
#endif

