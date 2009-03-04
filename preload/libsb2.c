/*
 * libsb2 -- scratchbox2 preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 *	Lauri T. Aarnio
 * 
 * Heavily based on the libfakechroot library by 
 * Piotr Roszatycki <dexter@debian.org>
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
#include <config_hardcoded.h>
#include <ctype.h>
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

/* String vector contents to a single string for logging.
 * returns pointer to an allocated buffer, caller should free() it.
*/
char *strvec_to_string(char *const *argv)
{
	int	result_max_size = 1;
	char	*const *vp;
	char	*buf;

	if (!argv) return(NULL);

	/* first, count max. size of the result */
	for (vp = argv; *vp; vp++) {
		/* add size of the string + one for the space + two for 
		 * the quotes (if needed) */
		result_max_size += strlen(*vp) + 3;
	}

	buf = malloc(result_max_size);
	*buf = '\0';

	/* next round: copy strings, using quotes for all strings that
	 * contain whitespaces. */
	for (vp = argv; *vp; vp++) {
		int	needs_quotes = 0;
		char	*cp = *vp;

		if (*cp) {
			/* non-empty string; see if it contains whitespace */
			while (*cp) {
				if (isspace(*cp)) {
					needs_quotes = 1;
					break;
				}
				cp++;
			}
		} else {
			/* empty string */
			needs_quotes = 1;
		}
		if (*buf) strcat(buf, " ");
		if (needs_quotes) strcat(buf, "'");
		strcat(buf, *vp);
		if (needs_quotes) strcat(buf, "'");
	}

	return(buf);
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
			SB_LOG(SB_LOGLEVEL_DEBUG, "EXEC: %s : %s", file, buf);
			free(buf);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"EXEC: %s (failed to print argv)", file);
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
		return execve_gate (NULL, realfnname, file, argv, envp);
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


#ifdef HAVE_FTS_H
/* FIXME: why there was #if !defined(HAVE___OPENDIR2) around fts_open() ???? */
FTS * fts_open_gate(FTS * (*real_fts_open_ptr)(char * const *path_argv,
		int options, int (*compar)(const FTSENT **,const FTSENT **)),
	const char *realfnname,
	char * const *path_argv,
	int options,
	int (*compar)(const FTSENT **,const FTSENT **))
{
	char *path;
	char * const *p;
	char **new_path_argv;
	char **np;
	int n;

	for (n=0, p=path_argv; *p; n++, p++);
	if ((new_path_argv = calloc(n+1, (sizeof(char *)))) == NULL) {
		return NULL;
	}

	for (n=0, p=path_argv, np=new_path_argv; *p; n++, p++, np++) {
		mapping_results_t res;

		clear_mapping_results_struct(&res);
		path = *p;
		sbox_map_path(realfnname, path,
			0/*dont_resolve_final_symlink*/, &res);
		if (res.mres_result_path) {
			/* Mapped OK */
			*np = strdup(res.mres_result_path);
		} else {
			*np = strdup("");
		}
		free_mapping_results(&res);
	}

	/* FIXME: this system causes memory leaks */

	return (*real_fts_open_ptr)(new_path_argv, options, compar);
}
#endif

char * get_current_dir_name_gate(
	char * (*real_get_current_dir_name_ptr)(void),
	const char *realfnname)
{
	char *sbox_path = NULL;
	char *cwd;

	if ((cwd = (*real_get_current_dir_name_ptr)()) == NULL) {
		return NULL;
	}
	if (*cwd != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, cwd);
	}
	if (sbox_path) {
		free(cwd);
		return sbox_path;
	}
	return(cwd); /* failed to reverse it */
}


/* #include <unistd.h> */
char *getcwd_gate (
	char *(*real_getcwd_ptr)(char *buf, size_t size),
	const char *realfnname,
	char *buf,
	size_t size)
{
	char *sbox_path = NULL;
	char *cwd;

	if ((cwd = (*real_getcwd_ptr)(buf, size)) == NULL) {
		return NULL;
	}
	if (*cwd != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, cwd);
	}
	if (sbox_path) {
SB_LOG(SB_LOGLEVEL_DEBUG, "GETCWD: '%s'", sbox_path);
		if(buf) {
			if (strlen(sbox_path) >= size) {
				/* path does not fit to the buffer */
				free(sbox_path);
				errno = ERANGE;
				return(NULL);
			}
			strncpy(buf, sbox_path, size);
			free(sbox_path);
		} else {
			/* buf==NULL: real getcwd() used malloc() to 
			 * allocate cwd (some implementations) [or the
			 * behavior may be unspecified (posix definition)]
			 * Assume memory was allocated, because the real
			 * getcwd() already returned a pointer to us...
			*/
			free(cwd);
			cwd = sbox_path;
		}
	}
SB_LOG(SB_LOGLEVEL_DEBUG, "GETCWD: returns '%s'", cwd);
	return cwd;
}


char * getwd_gate(
	char *(*real_getwd_ptr)(char *buf),
	const char *realfnname,
	char *buf)
{
	char *sbox_path = NULL;
	char *cwd;

	if ((cwd = (*real_getwd_ptr)(buf)) == NULL) {
		return NULL;
	}
	if (*cwd != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, cwd);
	}
	if (sbox_path) {
		if(buf) {
			if (strlen(sbox_path) >= PATH_MAX) {
				free(sbox_path);
				return(NULL);
			}
			strcpy(buf, sbox_path);
			free(sbox_path);
		} else {
			/* buf==NULL: next_getwd used malloc() to allocate cwd */
			free(cwd);
			cwd = sbox_path;
		}
	}
	return cwd;
}

char *realpath_gate(
	char *(*real_realpath_ptr)(const char *name, char *resolved),
        const char *realfnname,
	const char *name,	/* name, already mapped */
	char *resolved)
{
	char *sbox_path = NULL;
	char *rp;
	
	if ((rp = (*real_realpath_ptr)(name,resolved)) == NULL) {
		return NULL;
	}
	if (*rp != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, rp);
		if (sbox_path) {
			if (resolved) {
				strncpy(resolved, sbox_path, PATH_MAX);
				rp = resolved;
				free(sbox_path);
			} else {
				/* resolved was null - assume that glibc 
				 * allocated memory */
				free(rp);
				rp = sbox_path;
			}
		} /* else not reversed, just return rp */
	}
SB_LOG(SB_LOGLEVEL_DEBUG, "REALPATH: returns '%s'", rp);
	return(rp);
}


/* "SB2_WRAP_GLOB" needs to be defined on systems where the C library
 * is not based on glibc (or not compatible with glob() from glibc 2.7)
*/
#ifdef SB2_WRAP_GLOB
static char *check_and_prepare_glob_pattern(
	const char *realfnname,
	const char *pattern)
{
	char *mapped__pattern = NULL;

	/* only map pattern if it contains a '/'
	 * FIXME: this is probably not completely correct way to process this,
	 * a bit more intelligence could be useful here... that is why we'll
	 * log the mapped pattern (NOTICE level) if it was mapped
	*/
	if (*pattern == '/') { /* if absolute path in pattern.. */
		mapped__pattern = sbox_map_path(realfnname, pattern,
			NULL/*RO-flag*/, 0/*dont_resolve_final_symlink*/);
		if (!strcmp(mapped__pattern, pattern)) {
			/* no change */
			free(mapped__pattern);
			return(NULL);
		}
		SB_LOG(SB_LOGLEVEL_NOTICE, "%s: mapped pattern '%s' => '%s'",
			realfnname, pattern, mapped__pattern);
	}
	return(mapped__pattern);
}
#endif

int glob_gate(
	int (*real_glob_ptr)(const char *pattern, int flags,
		int (*errfunc) (const char *,int), glob_t *pglob),
	const char *realfnname,
	const char *pattern, /* has been mapped */
	int flags,
	int (*errfunc) (const char *,int),
	glob_t *pglob)
{
	int rc;
#ifdef SB2_WRAP_GLOB
	char *mapped__pattern;

	mapped__pattern = check_and_prepare_glob_pattern(realfnname, pattern);
	rc = (*real_glob_ptr)(mapped__pattern ? mapped__pattern : pattern,
		flags, errfunc, pglob);
	if (mapped__pattern) free(mapped__pattern);
#else
	/* glob() has been replaced by a modified copy (from glibc) */
	unsigned int	i;

	(void)real_glob_ptr; /* not used */

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: pattern='%s' gl_offs=%d, flags=0x%X",
		realfnname, pattern, pglob->gl_offs, flags);
	rc = do_glob(pattern, flags, errfunc, pglob);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d (gl_pathc=%d)",
		realfnname, rc, pglob->gl_pathc);
	for (i=0; i < pglob->gl_pathc; i++) {
		char *cp = pglob->gl_pathv[i + pglob->gl_offs];
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: [%d='%s']",
			realfnname, i, cp ? cp : "<NULL>");
	}
#endif
	
	return rc;
}

#ifdef HAVE_GLOB64
int glob64_gate(
	int (*real_glob64_ptr)(const char *pattern,
		int flags, int (*errfunc) (const char *,int), glob64_t *pglob),
	const char *realfnname,
	const char *pattern, /* has been mapped */
	int flags,
	int (*errfunc) (const char *,int),
	glob64_t *pglob)
{
	int rc;
#ifdef SB2_WRAP_GLOB
	char *mapped__pattern;

	mapped__pattern = check_and_prepare_glob_pattern(realfnname, pattern);
	rc = (*real_glob64_ptr)(mapped__pattern ? mapped__pattern : pattern,
		flags, errfunc, pglob);
	if (mapped__pattern) free(mapped__pattern);
#else
	/* glob64() has been replaced by a modified copy (from glibc) */
	unsigned int i;

	(void)real_glob64_ptr; /* not used */

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: pattern='%s' gl_offs=%d, flags=0x%X",
		realfnname, pattern, pglob->gl_offs, flags);
	rc = do_glob64(pattern, flags, errfunc, pglob);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: returns %d (gl_pathc=%d)",
		realfnname, rc, pglob->gl_pathc);
	for (i=0; i < pglob->gl_pathc; i++) {
		char *cp = pglob->gl_pathv[i + pglob->gl_offs];
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: [%d='%s']",
			realfnname, i, cp ? cp : "<NULL>");
	}
#endif

	return rc;
}
#endif

/* FIXME: The following two functions do not have anything to do with path
 * remapping. Instead these implementations prevent locking of the shadow
 * file, which I find really hard to understand. Please explain
 * why we should have these wrappers, or should these be removed completely?
 * (this same comment is in interface.master, so hopefully somebody
 * will be able to explain this someday!)
*/
/* #include <shadow.h> */
int lckpwdf (void)
{
	return 0;
}
int ulckpwdf (void)
{
	return 0;
}


int uname_gate(
	int (*real_uname_ptr)(struct utsname *buf),
	const char *realfnname,
	struct utsname *buf)
{
	static char *uname_machine = NULL;

	(void)realfnname;	/* not used here */

	if ((*real_uname_ptr)(buf) < 0) {
		return -1;
	}

	if (sbox_session_dir) {
		/* sb2 has been initialized. */
		if (!uname_machine || !*uname_machine) {
			uname_machine = sb2__read_string_variable_from_lua__(
				"sbox_uname_machine");
		}
		if (uname_machine && *uname_machine)
			snprintf(buf->machine, sizeof(buf->machine),
					"%s", uname_machine);
	}
	return 0;
}

void _exit_gate(void (*real__exit_ptr)(int status),
	const char *realfnname, int status)
{
	/* NOTE: Following SB_LOG() call is used by the log
	 *       postprocessor script "sb2-logz". Do not change
	 *       without making a corresponding change to the script!
	*/
	SB_LOG(SB_LOGLEVEL_INFO, "%s: status=%d", realfnname, status);
	(real__exit_ptr)(status);
}


void _Exit_gate(void (*real__Exit_ptr)(int status),
	const char *realfnname, int status)
{
	/* NOTE: Following SB_LOG() call is used by the log
	 *       postprocessor script "sb2-logz". Do not change
	 *       without making a corresponding change to the script!
	*/
	SB_LOG(SB_LOGLEVEL_INFO, "%s: status=%d", realfnname, status);
	(real__Exit_ptr)(status);
}
//void _Exit_gate() __attribute__ ((noreturn));

/* ---------- Socket API ---------- */

static void map_sockaddr_un(
	const char *realfnname,
	struct sockaddr_un *orig_serv_addr_un,
	struct sockaddr_un *mapped_serv_addr_un)
{
	mapping_results_t	res;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: checking AF_UNIX addr '%s'",
		realfnname, orig_serv_addr_un->sun_path);

	clear_mapping_results_struct(&res);
	/* FIXME: implement if(pathname_is_readonly!=0)... */
	sbox_map_path(realfnname, orig_serv_addr_un->sun_path,
		0/*dont_resolve_final_symlink*/, &res);
	if (res.mres_result_path == NULL) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: Failed to map AF_UNIX address '%s'",
			realfnname, orig_serv_addr_un->sun_path);
	} else {
		*mapped_serv_addr_un = *orig_serv_addr_un;
		if (sizeof(mapped_serv_addr_un->sun_path) <=
		    strlen(res.mres_result_path)) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Mapped AF_UNIX address (%s) is too long",
				realfnname, res.mres_result_path);
		} else {
			strcpy(mapped_serv_addr_un->sun_path,
				res.mres_result_path);
		}
	}
	free_mapping_results(&res);
}

int bind_gate(
	int (*real_bind_ptr)(int sockfd, const struct sockaddr *my_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *my_addr,
	socklen_t addrlen)
{
	if (my_addr->sa_family == AF_UNIX) {
		struct sockaddr_un mapped_my_addr_un;

		map_sockaddr_un(realfnname,
			(struct sockaddr_un*)my_addr, &mapped_my_addr_un);

		return((*real_bind_ptr)(sockfd,
			(struct sockaddr*)&mapped_my_addr_un,
			sizeof(mapped_my_addr_un)));
	} else {
		return((*real_bind_ptr)(sockfd, my_addr, addrlen));
	}
}

int connect_gate(
	int (*real_connect_ptr)(int sockfd, const struct sockaddr *serv_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *serv_addr,
	socklen_t addrlen)
{
	if (serv_addr->sa_family == AF_UNIX) {
		struct sockaddr_un mapped_serv_addr_un;

		map_sockaddr_un(realfnname,
			(struct sockaddr_un*)serv_addr, &mapped_serv_addr_un);

		return((*real_connect_ptr)(sockfd,
			(struct sockaddr*)&mapped_serv_addr_un,
			sizeof(mapped_serv_addr_un)));
	} else {
		return((*real_connect_ptr)(sockfd, serv_addr, addrlen));
	}
}

/* ---------- */

void *sbox_find_next_symbol(int log_enabled, const char *fn_name)
{
	char	*msg;
	void	*fn_ptr;

	if(log_enabled)
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %s", __func__, fn_name);

	fn_ptr = dlsym(RTLD_NEXT, fn_name);

	if ((msg = dlerror()) != NULL) {
		fprintf(stderr, "%s: dlsym(%s): %s\n",
			PACKAGE_NAME, fn_name, msg);
		if(log_enabled)
			SB_LOG(SB_LOGLEVEL_ERROR, "ERROR: %s: dlsym(%s): %s",
				PACKAGE_NAME, fn_name, msg);
		assert(0);
	}
	if (log_enabled)
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: %s at 0x%X", __func__,
			fn_name, (int)fn_ptr);
	return(fn_ptr);
}

/* ----- EXPORTED from interface.master: ----- */
char *sb2show__map_path2__(const char *binary_name, const char *mapping_mode, 
        const char *fn_name, const char *pathname, int *readonly)
{
	char *mapped__pathname = NULL;
	mapping_results_t mapping_result;

	(void)mapping_mode;	/* mapping_mode is not used anymore. */

	if (!sb2_global_vars_initialized__) sb2_initialize_global_variables();

	clear_mapping_results_struct(&mapping_result);
	if (pathname != NULL) {
		sbox_map_path_for_sb2show(binary_name, fn_name,
			pathname, &mapping_result);
		if (mapping_result.mres_result_path)
			mapped__pathname =
				strdup(mapping_result.mres_result_path);
		if (readonly) *readonly = mapping_result.mres_readonly;
	}
	free_mapping_results(&mapping_result);
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s '%s'", __func__, pathname);
	return(mapped__pathname);
}

char *sb2show__get_real_cwd__(const char *binary_name, const char *fn_name)
{
	char path[PATH_MAX];

	(void)binary_name;
	(void)fn_name;

	if (getcwd_nomap_nolog(path, sizeof(path))) {
		return(strdup(path));
	}
	return(NULL);
}

/* ---- support functions for the generated interface: */

/* returns true, if the "mode" parameter of fopen() (+friends)
 * indicates that write permission is required.
*/
int fopen_mode_w_perm(const char *mode)
{
	/* We'll have to look at the first characters only, since
	 * standard C allows implementation-specific additions to follow
	 * after the mode. "w", "a", "r+" and "rb+" need write access,
	 * but "r" or "r,access=lock" indicate read only access.
	 * For more information, see H&S 5th ed. p. 368
	*/
	if (*mode == 'w' || *mode == 'a') return (1);
	mode++;
	if (*mode == 'b') mode++;
	if (*mode == '+') return (1);
	return (0);
}

/* a helper function for freopen*(), which need to close the original stream
 * even if the open fails (e.g. this gets called when trying to open a R/O
 * mapped destination for R/W) */
int freopen_errno(FILE *stream)
{
	if (stream) fclose(stream);
	return (EROFS);
}

/* mkstemp() modifies "template". This locates the part which should be 
 * modified, and copies the modification back from mapped buffer (which
 * was modified by the real function) to callers buffer.
*/
static void postprocess_tempname_template(const char *realfnname,
	char *mapped__template, char *template)
{
	char *X_ptr;
	int mapped_len = strlen(mapped__template);
	int num_x = 0;

	X_ptr = strrchr(template, 'X'); /* point to last 'X' */
	if (!X_ptr) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: orig.template did not contain X (%s,%s), won't "
			"do anything", realfnname, template, mapped__template);
		return;
	}

	/* the last 'X' should be the last character in the template: */
	if (X_ptr[1] != '\0') {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: unknown orig.template format (%s,%s), "
			"won't do anything", 
			realfnname, template, mapped__template);
		return;
	}

	while ((X_ptr > template) && (X_ptr[-1] == 'X')) {
		X_ptr--;
	}

	/* now "X_ptr" points to the first 'X' to be modified.
	 * C standard says that the template should have six trailing 'X's.
	 * However, some systems seem to allow varying number of X characters
	 * (see the manual pages)
	*/
	num_x = strlen(X_ptr);

	if(mapped_len < num_x) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: mapped.template is too short (%s,%s), won't "
			"do anything", realfnname, template, mapped__template);
		return;
	}

	/* now copy last characters from mapping result to caller's buffer*/
	strncpy(X_ptr, mapped__template + (mapped_len-num_x), num_x);

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: template set to (%s)", realfnname, template);
}

void mkstemp_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template);
}

void mkstemp64_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template);
}

void mkdtemp_postprocess_template(const char *realfnname,
	char *ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template);
}

void mktemp_postprocess_template(const char *realfnname,
	char *ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template);
}

/* the real tmpnam() can not be used at all, because the generated name must
 * be mapped before the name can be tested and that won't happen inside libc.
 * Istead, we'll use mktemp()..
*/
char *tmpnam_gate(char *(*real_tmpnam_ptr)(char *s),
	 const char *realfnname, char *s)
{
	static char static_tmpnam_buf[PATH_MAX]; /* used if s is NULL */
	char tmpnam_buf[PATH_MAX];
	char *dir = getenv("TMPDIR");

	(void)real_tmpnam_ptr; /* not used */

	if (!dir) dir = P_tmpdir;
	if (!dir) dir = "/tmp";
	
	snprintf(tmpnam_buf, sizeof(tmpnam_buf), "%s/sb2-XXXXXX", dir);
	if (strlen(tmpnam_buf) >= L_tmpnam) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: tmp name (%s) >= %d",
			realfnname, tmpnam_buf, L_tmpnam);
	}

	if (mktemp(tmpnam_buf)) {
		/* success */
		if (s) {
			strcpy(s, tmpnam_buf);
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: result='%s'", realfnname, s);
			return(s);
		}
		
		/* s was NULL, return pointer to our static buffer */
		strcpy(static_tmpnam_buf, tmpnam_buf);
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: static buffer='%s'",
			realfnname, static_tmpnam_buf);
		return(static_tmpnam_buf);
	}
	/* mktemp() failed */
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: mktemp() failed", realfnname);
	return(NULL);
}

/* the real tempnam() can not be used, just like tmpnam() can't be used.
*/
char *tempnam_gate(
	char *(*real_tempnam_ptr)(const char *tmpdir, const char *prefix),
        const char *realfnname, const char *tmpdir, const char *prefix)
{
	const char *dir = NULL;
	int namelen;
	char *tmpnam_buf;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s / %s called", realfnname, __func__);

	(void)real_tempnam_ptr; /* not used */

	if (tmpdir) {
		dir = tmpdir;
	} else {
		dir = getenv("TMPDIR");
		if (!dir) dir = P_tmpdir;
		if (!dir) dir = "/tmp";
	}
	
	namelen = strlen(dir) + 1 + (prefix?strlen(prefix):4) + 6 + 1;

	tmpnam_buf = malloc(namelen);
	if (!tmpnam_buf) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: malloc() failed", realfnname);
		return(NULL);
	}
	snprintf(tmpnam_buf, namelen, "%s/%sXXXXXX", 
		dir, (prefix ? prefix : "tmp."));

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: namelen=%d buf='%s'", 
		__func__, namelen, tmpnam_buf);

	if (mktemp(tmpnam_buf) && *tmpnam_buf) {
		/* success */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"%s: result='%s'", realfnname, tmpnam_buf);
		
		return(tmpnam_buf);
	}
	/* mktemp() failed */
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: mktemp() failed", realfnname);
	return(NULL);
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

/* global variables, these come from the environment:
 * used to store session directory & ld.so settings.
 * set up as early as possible, so that if the application clears our
 * environment we'll still know the original values.
*/
char *sbox_session_dir = NULL;
char *sbox_session_mode = NULL; /* optional */
char *sbox_session_perm = NULL; /* optional */
char *sbox_orig_ld_preload = NULL;
char *sbox_orig_ld_library_path = NULL;
char *sbox_binary_name = NULL;
char *sbox_real_binary_name = NULL;
char *sbox_orig_binary_name = NULL;
char *sbox_active_exec_policy_name = NULL;

int sb2_global_vars_initialized__ = 0;

/* to be used from sb2-show only: */
void sb2__set_active_exec_policy_name__(const char *name)
{
	sbox_active_exec_policy_name = name ? strdup(name) : NULL;
}

/* sb2_initialize_global_variables()
 *
 * NOTE: This function can be called before the environment
 * is available. This happens at least on Linux when the
 * pthreads library is initializing; it calls uname(), which
 * will be handled by our uname() wrapper (it has been prepared
 * for this situation, too)
 *
 * WARNING: Never call the logger from this function
 * before "sb2_global_vars_initialized__" has been set!
 * (the result would be infinite recursion, because the logger wants
 * to open() the logfile..)
*/
void sb2_initialize_global_variables(void)
{
	if (!sb2_global_vars_initialized__) {
		char	*cp;

		if (!sbox_session_dir) {
			cp = getenv("SBOX_SESSION_DIR");
			if (cp) sbox_session_dir = strdup(cp);
		}
		if (!sbox_session_mode) {
			/* optional variable */
			cp = getenv("SBOX_SESSION_MODE");
			if (cp) sbox_session_mode = strdup(cp);
		}
		if (!sbox_session_perm) {
			/* optional variable */
			cp = getenv("SBOX_SESSION_PERM");
			if (cp) sbox_session_perm = strdup(cp);
		}
		if (!sbox_orig_ld_preload) {
			cp = getenv("LD_PRELOAD");
			if (cp) sbox_orig_ld_preload = strdup(cp);
		}
		if (!sbox_orig_ld_library_path) {
			cp = getenv("LD_LIBRARY_PATH");
			if (cp) sbox_orig_ld_library_path = strdup(cp);
		}
		if (!sbox_binary_name) {
			cp = getenv("__SB2_BINARYNAME");
			if (cp) sbox_binary_name = strdup(cp);
		}
		if (!sbox_real_binary_name) {
			cp = getenv("__SB2_REAL_BINARYNAME");
			if (cp) sbox_real_binary_name = strdup(cp);
		}
		if (!sbox_orig_binary_name) {
			cp = getenv("__SB2_ORIG_BINARYNAME");
			if (cp) sbox_orig_binary_name = strdup(cp);
		}
		if (!sbox_active_exec_policy_name) {
			cp = getenv("__SB2_EXEC_POLICY_NAME");
			if (cp) sbox_active_exec_policy_name = strdup(cp);
		}

		if (sbox_session_dir) {
			/* seems that we got it.. */
			sb2_global_vars_initialized__ = 1;
			SB_LOG(SB_LOGLEVEL_DEBUG, "global vars initialized from env");
		}
	}
}

