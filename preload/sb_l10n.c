/*
 * sb_l10n.c -- exec_policy based localization support.
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsb2.h"
#include "exported.h"

static char *message_catalog_prefix = NULL;
static int message_catalog_prefix_retrieved = 0;

static const char *get_message_catalog_prefix(void);
static void check_textdomain(const char *);

/*
 * Returns message catalog prefix for this binary.  This value
 * can be used to map text domain paths under target_root or
 * tools_root.  Returns NULL if there is no such specified.
 */
static const char *get_message_catalog_prefix(void)
{
	if (message_catalog_prefix_retrieved)
		return (message_catalog_prefix);

	if (!sb2_global_vars_initialized__)
		sb2_initialize_global_variables();

	/*
	 * There might be situations when sb2 starts where
	 * $__SB2_REAL_BINARYNAME is not set.  This happens
	 * on very first exec so we cannot apply any exec
	 * policy.
	 */
	if (sbox_real_binary_name == NULL)
		return (message_catalog_prefix);

	/*
	 * Now, try to fetch configured message catalog prefix
	 * from exec policy.  This is normally set to target_root
	 * or tools_root.  Note that we leak memory here as we
	 * never free the returned string (it could be NULL, though).
	 */
	message_catalog_prefix =
	    sb_query_exec_policy("native_app_message_catalog_prefix",
	    sbox_binary_name, sbox_real_binary_name);

	if (message_catalog_prefix != NULL) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
		    "native_app_message_catalog_prefix='%s'",
		    message_catalog_prefix);
	}

	message_catalog_prefix_retrieved = 1;
	return (message_catalog_prefix);
}

/*ARGSUSED*/
char *bindtextdomain_gate(char *(*realfn)(const char *, const char *),
    const char *realfn_name, const char *domainname, const char *dirname)
{
	const char *mapped_dirname = dirname;
	const char *message_catalog_prefix = NULL;
	char *message_catalog_path = NULL;
	char *result = NULL;

	(void)realfn_name;

	if (dirname == NULL)
		goto out;
	if ((message_catalog_prefix = get_message_catalog_prefix()) == NULL)
		goto out;

	/* is it already mapped? */
	if (strstr(dirname, message_catalog_prefix) != NULL)
		goto out; /* don't remap */

	/*
	 * Got message catalog prefix for this binary.  Next we
	 * bind given text domain under that prefix:
	 *     <prefix>/usr/share
	 * or whatever is was before.
	 */
	if (dirname[0] == '/') {
		(void) asprintf(&message_catalog_path, "%s%s",
		    message_catalog_prefix, dirname);
	} else {
		/* TODO: should we even allow relative paths? */
		(void) asprintf(&message_catalog_path, "%s/%s",
		    message_catalog_prefix, dirname);
	}
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "binding domain %s to %s",
	    domainname, message_catalog_path);
	mapped_dirname = message_catalog_path;

out:
	result = (*realfn)(domainname, mapped_dirname);
	free(message_catalog_path);
	return (result);
}

static void check_textdomain(const char *domainname)
{
	/*
	 * We read back current bindings for given domainname and
	 * call bindtextdomain() which then goes into our gate
	 * function that maps the path if exec policy says so.
	 */
	(void) bindtextdomain(domainname, bindtextdomain(domainname, NULL));
}

/*ARGSUSED*/
char *textdomain_gate(char *(*realfn)(const char *), const char *realfn_name,
    const char *domainname)
{
	(void)realfn_name;

	check_textdomain(domainname);
	return (*realfn)(domainname);
}

/*ARGSUSED*/
char *dgettext_gate(char *(*realfn)(const char *, const char *),
    const char *realfn_name, const char *domainname, const char *msgid)
{
	(void)realfn_name;

	check_textdomain(domainname);
	return (*realfn)(domainname, msgid);
}

/*ARGSUSED*/
char *dcgettext_gate(char *(*realfn)(const char *, const char *, int),
    const char *realfn_name, const char *domainname, const char *msgid,
    int category)
{
	(void)realfn_name;

	check_textdomain(domainname);
	return (*realfn)(domainname, msgid, category);
}

/*ARGSUSED*/
char *dngettext_gate(char *(*realfn)(const char *, const char *, const char *,
    unsigned long int n), const char *realfn_name,
    const char *domainname, const char *msgid,
    const char *msgid_plural, unsigned long int n)
{
	(void)realfn_name;

	check_textdomain(domainname);
	return (*realfn)(domainname, msgid, msgid_plural, n);
}

/*ARGSUSED*/
char *dcngettext_gate(char *(*realfn)(const char *, const char *, const char *,
    unsigned long int, int), const char *realfn_name,
    const char *domainname, const char *msgid,
    const char *msgid_plural, unsigned long int n, int category)
{
	(void)realfn_name;

	check_textdomain(domainname);
	return (*realfn)(domainname, msgid, msgid_plural, n, category);
}
