/*
 * libsb2 -- misc. GATE fucntions of the scratchbox2 preload library
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

#ifdef HAVE_FTS_H
/* FIXME: why there was #if !defined(HAVE___OPENDIR2) around fts_open() ???? */
FTS * fts_open_gate(
	int *result_errno_ptr,
	FTS * (*real_fts_open_ptr)(char * const *path_argv,
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
	FTS *result;

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

	errno = *result_errno_ptr; /* restore to orig.value */
	result = (*real_fts_open_ptr)(new_path_argv, options, compar);
	*result_errno_ptr = errno;
	return(result);
}
#endif

char * get_current_dir_name_gate(
	int *result_errno_ptr,
	char * (*real_get_current_dir_name_ptr)(void),
	const char *realfnname)
{
	char *sbox_path = NULL;
	char *cwd;

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((cwd = (*real_get_current_dir_name_ptr)()) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	if (*cwd != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, cwd);
	}
	if (sbox_path) {
		free(cwd);
		return sbox_path;
	}
	return(cwd); /* failed to reverse it */
}

static char *getcwd_common(char *buf, size_t size,
	const char *realfnname, char *cwd)
{
	char *sbox_path = NULL;

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

/* #include <unistd.h> */
char *getcwd_gate (
	int *result_errno_ptr,
	char *(*real_getcwd_ptr)(char *buf, size_t size),
	const char *realfnname,
	char *buf,
	size_t size)
{
	char *cwd;

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((cwd = (*real_getcwd_ptr)(buf, size)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	return(getcwd_common(buf, size, realfnname, cwd));
}

char *__getcwd_chk_gate(
	int *result_errno_ptr,
	char * (*real___getcwd_chk_ptr)(char *buf,
		size_t size, size_t buflen),
        const char *realfnname,
	char *buf,
	size_t size,
	size_t buflen)
{
	char *cwd;

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((cwd = (*real___getcwd_chk_ptr)(buf, size, buflen)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	return(getcwd_common(buf, size, realfnname, cwd));
}

static char *getwd_common(char *cwd, const char *realfnname, char *buf)
{
	char *sbox_path = NULL;

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

char *getwd_gate(
	int *result_errno_ptr,
	char *(*real_getwd_ptr)(char *buf),
	const char *realfnname,
	char *buf)
{
	char *cwd;

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((cwd = (*real_getwd_ptr)(buf)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	return(getwd_common(buf, realfnname, cwd));
}

char *__getwd_chk_gate(
	int *result_errno_ptr,
	char * (*real___getwd_chk_ptr)(char *buf,
		size_t buflen),
        const char *realfnname,
	char *buf,
	size_t buflen)
{
	char *cwd;

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((cwd = (*real___getwd_chk_ptr)(buf, buflen)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	return(getwd_common(buf, realfnname, cwd));
}

char *realpath_gate(
	int *result_errno_ptr,
	char *(*real_realpath_ptr)(const char *name, char *resolved),
        const char *realfnname,
	const char *name,	/* name, already mapped */
	char *resolved)
{
	char *sbox_path = NULL;
	char *rp;
	
	errno = *result_errno_ptr; /* restore to orig.value */
	if ((rp = (*real_realpath_ptr)(name,resolved)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
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
	SB_LOG(SB_LOGLEVEL_NOISE, "REALPATH: returns '%s'", rp);
	return(rp);
}

/* gate for 
 *     char *__realpath_chk (__const char *__restrict __name,
 *		char *__restrict __resolved, size_t __resolvedlen)
 * (__realpath_chk is yet another ugly trick from the creators of glibc)
*/
char *__realpath_chk_gate(
	int *result_errno_ptr,
	char *(*real__realpath_chk_ptr)(__const char *__restrict __name,
		char *__restrict __resolved, size_t __resolvedlen),
        const char *realfnname,
	__const char *__restrict name,	/* name, already mapped */
	char *__restrict __resolved,
	size_t __resolvedlen)
{
	char *sbox_path = NULL;
	char *rp;
	
	errno = *result_errno_ptr; /* restore to orig.value */
	if ((rp = (*real__realpath_chk_ptr)(name,__resolved,__resolvedlen)) == NULL) {
		*result_errno_ptr = errno;
		return NULL;
	}
	*result_errno_ptr = errno;
	if (*rp != '\0') {
		sbox_path = scratchbox_reverse_path(realfnname, rp);
		if (sbox_path) {
			if (__resolved) {
				strncpy(__resolved, sbox_path, __resolvedlen);
				rp = __resolved;
				free(sbox_path);
			} else {
				/* resolved was null - assume that glibc 
				 * allocated memory */
				free(rp);
				rp = sbox_path;
			}
		} /* else not reversed, just return rp */
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "REALPATH: returns '%s'", rp);
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
	int *result_errno_ptr,
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
	errno = *result_errno_ptr; /* restore to orig.value */
	rc = (*real_glob_ptr)(mapped__pattern ? mapped__pattern : pattern,
		flags, errfunc, pglob);
	*result_errno_ptr = errno;
	if (mapped__pattern) free(mapped__pattern);
#else
	/* glob() has been replaced by a modified copy (from glibc) */
	unsigned int	i;

	(void)real_glob_ptr; /* not used */

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: pattern='%s' gl_offs=%d, flags=0x%X",
		realfnname, pattern, pglob->gl_offs, flags);
	errno = *result_errno_ptr; /* restore to orig.value */
	rc = do_glob(pattern, flags, errfunc, pglob);
	*result_errno_ptr = errno;
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
	int *result_errno_ptr,
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
	errno = *result_errno_ptr; /* restore to orig.value */
	rc = (*real_glob64_ptr)(mapped__pattern ? mapped__pattern : pattern,
		flags, errfunc, pglob);
	*result_errno_ptr = errno;
	if (mapped__pattern) free(mapped__pattern);
#else
	/* glob64() has been replaced by a modified copy (from glibc) */
	unsigned int i;

	(void)real_glob64_ptr; /* not used */

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: pattern='%s' gl_offs=%d, flags=0x%X",
		realfnname, pattern, pglob->gl_offs, flags);
	errno = *result_errno_ptr; /* restore to orig.value */
	rc = do_glob64(pattern, flags, errfunc, pglob);
	*result_errno_ptr = errno;
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


int uname_gate(
	int *result_errno_ptr,
	int (*real_uname_ptr)(struct utsname *buf),
	const char *realfnname,
	struct utsname *buf)
{
	static char *uname_machine = NULL;

	(void)realfnname;	/* not used here */

	errno = *result_errno_ptr; /* restore to orig.value */
	if ((*real_uname_ptr)(buf) < 0) {
		*result_errno_ptr = errno;
		return -1;
	}
	*result_errno_ptr = errno;

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

void _exit_gate(
	int *result_errno_ptr,
	void (*real__exit_ptr)(int status),
	const char *realfnname, int status)
{
	(void)result_errno_ptr; /* not used */

	/* NOTE: Following SB_LOG() call is used by the log
	 *       postprocessor script "sb2-logz". Do not change
	 *       without making a corresponding change to the script!
	*/
	SB_LOG(SB_LOGLEVEL_INFO, "%s: status=%d", realfnname, status);
	(real__exit_ptr)(status);
}


void _Exit_gate(
	int *result_errno_ptr,
	void (*real__Exit_ptr)(int status),
	const char *realfnname, int status)
{
	(void)result_errno_ptr; /* not used */

	/* NOTE: Following SB_LOG() call is used by the log
	 *       postprocessor script "sb2-logz". Do not change
	 *       without making a corresponding change to the script!
	*/
	SB_LOG(SB_LOGLEVEL_INFO, "%s: status=%d", realfnname, status);
	(real__Exit_ptr)(status);
}
//void _Exit_gate() __attribute__ ((noreturn));

