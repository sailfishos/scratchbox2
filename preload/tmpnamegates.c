/*
 * libsb2 -- mkstemp(), mktemp() etc. GATEs and postprocessors
 *           of the scratchbox2 preload library
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

/* mkstemp() modifies "template". This locates the part which should be 
 * modified, and copies the modification back from mapped buffer (which
 * was modified by the real function) to callers buffer.
*/
static void postprocess_tempname_template(const char *realfnname,
	char *mapped__template, char *template, int suffixlen)
{
	char *X_ptr;
	int mapped_len = strlen(mapped__template);
	int template_len = strlen(template);
	int num_x = 0;

	if (suffixlen >= template_len) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: template length too long %d >= %d, ignoring",
			realfnname, suffixlen, template_len);
		return;
	}

	/* find X, working backwards along the template from the suffix */
	X_ptr = template + template_len - suffixlen;
	while (*X_ptr != 'X' && X_ptr > template)
		X_ptr --;

	if (*X_ptr != 'X') {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: orig.template did not contain X (%s,%s), won't "
			"do anything", realfnname, template, mapped__template);
		return;
	}

	/* the last 'X' should be the last character in the template: */
	if (X_ptr[suffixlen + 1] != '\0') {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: unknown orig.template format (%s,%s), "
			"won't do anything",
			realfnname, template, mapped__template);
		return;
	}

	num_x = 1;
	while ((X_ptr > template) && (X_ptr[-1] == 'X')) {
		X_ptr--;
		num_x ++;
	}

	/* now "X_ptr" points to the first 'X' to be modified.
	 * C standard says that the template should have six trailing 'X's.
	 * However, some systems seem to allow varying number of X characters
	 * (see the manual pages)
	*/

	if (mapped_len < num_x) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"%s: mapped.template is too short (%s,%s), won't "
			"do anything", realfnname, template, mapped__template);
		return;
	}

	/* now copy last characters from mapping result to caller's buffer*/
	memcpy(X_ptr, mapped__template + (mapped_len-num_x-suffixlen), num_x);

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: template set to (%s)", realfnname, template);
}

void mkstemp_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, 0);
}

void mkstemp64_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, 0);
}

void mkdtemp_postprocess_template(const char *realfnname,
	char *ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, 0);
}

void mktemp_postprocess_template(const char *realfnname,
	char *ret, mapping_results_t *res, char *template)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, 0);
}

void mkstemps_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template, int suffixlen)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, suffixlen);
}

void mkstemps64_postprocess_template(const char *realfnname,
	int ret, mapping_results_t *res, char *template, int suffixlen)
{
	(void)ret;
	postprocess_tempname_template(realfnname, res->mres_result_path, template, suffixlen);
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

