/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 *
 * License: LGPL-2.1
 *
 * ----------------
*/

#if 0
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <string.h>
#include <libgen.h>
#define _GNU_SOURCE
#else
#include <string.h>
#include <libgen.h>
#endif

#include <limits.h>
#include <sys/param.h>
#include <sys/file.h>
#include <assert.h>
#include <pthread.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include <mapping.h>
#include <sb2.h>
#include "libsb2.h"
#include "exported.h"

#include "pathmapping.h" /* get private definitions of this subsystem */

/* ========== Path & Path component handling primitives: ========== */

void set_flags_in_path_entries(struct path_entry *pep, int flags)
{
	while (pep) {
		pep->pe_flags |= flags;
		pep = pep->pe_next;
	}
}

struct path_entry *append_path_entries(
	struct path_entry *head,
	struct path_entry *new_entries)
{
	struct path_entry *work = head;
	
	if (!head) return(new_entries);

	/* move work to point to last component of "head" */
	while (work->pe_next) work = work->pe_next;

	work->pe_next = new_entries;
	if (new_entries) new_entries->pe_prev = work;

	return (head);
}

/* returns an allocated buffer */
char *path_entries_to_string_until(
	const struct path_entry *p_entry,
	const struct path_entry *last_path_entry_to_include,
	int flags)
{
	char *buf;
	const struct path_entry *work;
	int len;

	if (!p_entry) {
		/* "p_entry" will be empty if orig.path was "/." */
		return(strdup("/"));
	}

	/* first, count length of the buffer */
	work = p_entry;
	len = 0;
	while (work) {
		len++; /* "/" */
		len += work->pe_path_component_len;
		if (work == last_path_entry_to_include) break;
		work = work->pe_next;
	}
	len += 2; /* for trailing (optional) '/' and \0 */

	buf = malloc(len);
	*buf = '\0';

	/* add path components to the buffer */
	work = p_entry;
	if (flags & PATH_FLAGS_ABSOLUTE) {
		strcpy(buf, "/");
	}
	while (work) {
		int component_is_empty;
		if (*work->pe_path_component) {
			strcat(buf, work->pe_path_component);
			component_is_empty = 0;
		} else {
			component_is_empty = 1;
		}
		if (work == last_path_entry_to_include) break;
		work = work->pe_next;
		if (work && (component_is_empty==0)) {
			strcat(buf, "/");
		}
	}
	if (flags & PATH_FLAGS_HAS_TRAILING_SLASH) {
		char *cp = buf;
		/* point to last char in buf */
		while (*cp && cp[1]) cp++; 
		if (*cp != '/') strcat(buf, "/");
	}

	return(buf);
}

char *path_entries_to_string(
	const struct path_entry *p_entry,
	int flags)
{
	return(path_entries_to_string_until(p_entry, NULL, flags));
}

char *path_list_to_string(const struct path_entry_list *listp)
{
	return(path_entries_to_string_until(listp->pl_first, NULL,
		listp->pl_flags));
}

static void free_path_entry(struct path_entry *work)
{
	SB_LOG(SB_LOGLEVEL_NOISE3,
		"free_path_entry %lX <p=%lX n=%lX> len=%d '%s' (link_dest=%s)",
		(long)work, (long)work->pe_prev, (long)work->pe_next,
		work->pe_path_component_len, work->pe_path_component,
		(work->pe_link_dest ? work->pe_link_dest : NULL));
	if (work->pe_link_dest) free(work->pe_link_dest);
	free(work);
}

void free_path_entries(struct path_entry *work)
{
	while (work) {
		struct path_entry *next = work->pe_next;
		free_path_entry(work);
		work = next;
	}
}

void free_path_list(struct path_entry_list *listp)
{
	free_path_entries(listp->pl_first);
	clear_path_entry_list(listp);
}


struct path_entry *split_path_to_path_entries(
	const char *cpath, int *flagsp)
{
	struct path_entry *first = NULL;
	struct path_entry *work = NULL;
	const char *start;
	const char *next_slash;
	int flags = 0;

	SB_LOG(SB_LOGLEVEL_NOISE3, "going to split '%s'", cpath);

	start = cpath;
	if (*start == '/') {
		while(*start == '/') start++;	/* ignore leading '/' */
		flags |= PATH_FLAGS_ABSOLUTE;
	}

	do {
		int	len;

		next_slash = strchr(start, '/');
		if (next_slash) {
			len = next_slash - start;
		} else {
			/* no more slashes */
			len = strlen(start);
		}

		/* ignore empty strings resulting from // */
		if (len == 0) {
			/* but notice if there is trailing slash */
			if (!next_slash)
				flags |= PATH_FLAGS_HAS_TRAILING_SLASH;
		} else {
			struct path_entry *new;

			new = malloc(sizeof(struct path_entry) + len);
			if(!first) first = new;
			if (!new) abort();
			memset(new, 0, sizeof(struct path_entry));
			strncpy(new->pe_path_component, start, len);
			new->pe_path_component[len] = '\0';
			new->pe_path_component_len = len;

			new->pe_prev = work;
			if(work) work->pe_next = new;
			new->pe_next = NULL;
			work = new;
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"created entry 0x%lX '%s'",
				(unsigned long int)work, new->pe_path_component);
		}
		if (next_slash) start = next_slash + 1;
	} while (next_slash != NULL);

	if (flagsp) *flagsp = flags;
	return (first);
}

void split_path_to_path_list(
	const char *cpath,
	struct path_entry_list	*listp)
{
	listp->pl_first = split_path_to_path_entries(cpath, &(listp->pl_flags));

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE2)) {
		char *tmp_path_buf = path_list_to_string(listp);

		SB_LOG(SB_LOGLEVEL_NOISE2, "split->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
}

struct path_entry *duplicate_path_entries_until(
	const struct path_entry *duplicate_until_this_component,
	const struct path_entry *source_path)
{
	struct path_entry *first = NULL;
	struct path_entry *dest_path_ptr = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE3, "Duplicating path:");

	while (source_path) {
		struct path_entry *new;
		int	len = source_path->pe_path_component_len;

		new = malloc(sizeof(struct path_entry) + len);
		if (!new) abort();
		if(!first) first = new;
		memset(new, 0, sizeof(struct path_entry) + len);

		strncpy(new->pe_path_component, source_path->pe_path_component, len);
		new->pe_path_component[len] = '\0';
		new->pe_path_component_len = len;

		if (source_path->pe_link_dest)
			new->pe_link_dest = strdup(source_path->pe_link_dest);

		new->pe_prev = dest_path_ptr;
		if (dest_path_ptr) dest_path_ptr->pe_next = new;
		new->pe_next = NULL;
		dest_path_ptr = new;
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"dup: entry 0x%X '%s'",
			(unsigned long int)dest_path_ptr, new->pe_path_component);
		if (duplicate_until_this_component == source_path) break;
		source_path = source_path->pe_next;
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		char *tmp_path_buf = path_entries_to_string(first, 0);

		SB_LOG(SB_LOGLEVEL_NOISE3, "dup->'%s'", tmp_path_buf);
		free(tmp_path_buf);
	}
	return(first);
}

void	duplicate_path_list_until(
	const struct path_entry *duplicate_until_this_component,
	struct path_entry_list *new_path_list,
	const struct path_entry_list *source_path_list)
{
	struct path_entry *duplicate = NULL;

	duplicate = duplicate_path_entries_until(
		duplicate_until_this_component, source_path_list->pl_first);

	new_path_list->pl_first = duplicate;
	new_path_list->pl_flags = source_path_list->pl_flags;
}

/* remove a path_entry from list, return pointer to the next
 * path_entry after the removed one (NULL if the removed entry was last)
*/
struct path_entry *remove_path_entry(
	struct path_entry_list *listp,
	struct path_entry *p_entry)	/* entry to be removed */
{
	struct path_entry *ret = p_entry->pe_next;

	SB_LOG(SB_LOGLEVEL_NOISE3,
		"remove_path_entry at %lX, next=%lX",
		(long)p_entry, (long)ret);
	if (p_entry->pe_prev) {
		/* not the first element in the list */
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"remove_path_entry, not first");
		p_entry->pe_prev->pe_next = p_entry->pe_next;
		if(p_entry->pe_next)
			p_entry->pe_next->pe_prev = p_entry->pe_prev;
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"remove_path_entry, first");
		/* removing first element from the list */
		assert(p_entry == listp->pl_first);
		listp->pl_first = p_entry->pe_next;
		if(p_entry->pe_next)
			p_entry->pe_next->pe_prev = NULL;
	}
	free_path_entry(p_entry);
	return(ret);
}

/* check if the path is clean:
 * returns
 *   0: path is clean = does not contain "." or ".."
 *   1: path is not clean, contains one or more dots ("."), but no ".."
 *   2: path is not clean, ".." (requires more complex cleanup)
*/
int is_clean_path(struct path_entry_list *listp)
{
	struct path_entry *work = listp->pl_first;
	int	found_dot = 0;

	/* check if the path contains "." or ".." as components */
	while (work) {
		const char	*cp = work->pe_path_component;
		if (*cp == '.') {
			if (cp[1] == '\0') {
				found_dot = 1;
			} else if (cp[1] == '.' && cp[2] == '\0') {
				/* found ".." */
				SB_LOG(SB_LOGLEVEL_NOISE,
					"is_clean_path: dirty, found ..");
				return(2);
			}
		}
		work = work->pe_next;
	}

	if (found_dot) {
		SB_LOG(SB_LOGLEVEL_NOISE, "is_clean_path: dirty, found .");
		return(1);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "is_clean_path: clean");
	return(0);
}

char *clean_and_log_fs_mapping_result(
	const path_mapping_context_t *ctx,
	const char *abs_clean_virtual_path,
	int result_log_level, char *host_path,
	int flags)
{
	/* sometimes a mapping rule may create paths that contain
	 * doubled slashes ("//") or end with a slash. We'll
	 * need to clean the path here.
	*/
	char *cleaned_host_path;
	struct path_entry_list list;
	const char *readonly = "";

	split_path_to_path_list(host_path, &list);
	list.pl_flags|= PATH_FLAGS_HOST_PATH;

	switch (is_clean_path(&list)) {
	case 0: /* clean */
		break;
	case 1: /* . */
		remove_dots_from_path_list(&list);
		break;
	case 2: /* .. */
		/* The rule inserted ".." to the path?
		 * not very wise move, maybe we should even log
		 * warning about this? However, cleaning is
		 * easy in this case; the result is a host
		 * path => cleanup doesn't need to make
		 * recursive calls to sb_path_resolution.
		*/
		remove_dots_from_path_list(&list);
		if (clean_dotdots_from_path(ctx, &list)) {
			SB_LOG(result_log_level, "fail: %s '%s'",
				ctx->pmc_func_name, abs_clean_virtual_path);
			free_path_list(&list);
			return(NULL);
		}
		break;
	}
	cleaned_host_path = path_list_to_string(&list);
	free_path_list(&list);

	if (*cleaned_host_path != '/') {
		/* oops, got a relative path. CWD is too long. */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"OOPS, call_lua_function_sbox_translate_path:"
			" relative");
	}

	/* log the result */
	if (flags & SB2_MAPPING_RULE_FLAGS_READONLY_FS_IF_NOT_ROOT) {
		readonly = " (readonly-if-not-root)";
	} else if (flags & (SB2_MAPPING_RULE_FLAGS_READONLY |
			    SB2_MAPPING_RULE_FLAGS_READONLY_FS_ALWAYS)) {
		readonly = " (readonly)";
	}
	if (strcmp(cleaned_host_path, abs_clean_virtual_path) == 0) {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to
		 *       the script!
		*/
		SB_LOG(result_log_level, "pass: %s '%s'%s",
			ctx->pmc_func_name, abs_clean_virtual_path, readonly);
	} else {
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to
		 *       the script!
		*/
		SB_LOG(result_log_level, "mapped: %s '%s' -> '%s'%s",
			ctx->pmc_func_name, abs_clean_virtual_path,
			cleaned_host_path, readonly);
	}
	return (cleaned_host_path);
}

