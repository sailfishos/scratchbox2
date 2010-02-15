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
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include "libsb2.h"
#include "exported.h"

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

/* global variables, these come from the environment:
 * used to store session directory & ld.so settings.
 * set up as early as possible, so that if the application clears our
 * environment we'll still know the original values.
*/
char *sbox_session_dir = NULL;
char *sbox_session_mode = NULL; /* optional */
char *sbox_session_perm = NULL; /* optional */
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

static void dump_environ_to_log(const char *msg)
{
	char *buf = strvec_to_string(environ);

	if (buf) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: '%s'",
			msg, buf);
		free(buf);
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ENV: (failed to create string)");
	}
}

static void revert_to_user_version_of_env_var(
	const char *realvarname, const char *wrappedname)
{
	char *cp = getenv(wrappedname);

	if (cp) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "Revert env.var:%s=%s (%s)",
			realvarname, cp, wrappedname);
		setenv(realvarname, cp, 1/*overwrite*/);
	} else {
		int r;
		SB_LOG(SB_LOGLEVEL_DEBUG, "Revert env.var:Clear %s", realvarname);
		r = unsetenv(realvarname);
		if(r < 0) {
			int e = errno;
			SB_LOG(SB_LOGLEVEL_ERROR, "unsetenv(%s) failed, errno=%d",
				realvarname, e);
		}
	}
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3))
		dump_environ_to_log("revert_to_user_version_of_env_var: env. is now:");
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
			sblog_init();
			SB_LOG(SB_LOGLEVEL_DEBUG, "global vars initialized from env");

			/* check if the user wants us to SIGTRAP
			 * during libsb2 initialization.
			 *
			 * This is used together with two small GDB
			 * modification to make running gdb possible
			 * in sb2 emulation mode.
			 *
			 * The needed modifications in GDB:
			 *   - START_INFERIOR_TRAPS_EXPECTED=4 in gdb/inferior.h
			 *   - 'set environment SBOX_SIGTRAP' at startup
			*/
			if (getenv("SBOX_SIGTRAP"))
				raise(SIGTRAP);

			/* now when we know that the environment is
			 * valid, it is time to change LD_PRELOAD and
			 * LD_LIBRARY_PATH back to the values that the
			 * user expects to see.
			*/
			revert_to_user_version_of_env_var("LD_PRELOAD", "__SB2_LD_PRELOAD");
			revert_to_user_version_of_env_var("LD_LIBRARY_PATH", "__SB2_LD_LIBRARY_PATH");
		}
	}
}

