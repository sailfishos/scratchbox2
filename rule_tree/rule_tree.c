/*
 * Copyright (C) 2011 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include <sys/mman.h>

#include "rule_tree.h"

static struct ruletree_cxt_s {
	char		*rtree_ruletree_path;
	int		rtree_ruletree_fd;
	size_t		rtree_ruletree_file_size;
	void		*rtree_ruletree_ptr;
	ruletree_hdr_t	*rtree_ruletree_hdr_p;
} ruletree_ctx = { NULL, -1, 0, NULL, NULL };

/* =================== Rule tree primitives. =================== */

size_t ruletree_get_file_size(void)
{
	return (ruletree_ctx.rtree_ruletree_file_size);
}

/* return a pointer to the rule tree, without checking the contents */
static void *offset_to_raw_ruletree_ptr(ruletree_object_offset_t offs)
{
	if (!ruletree_ctx.rtree_ruletree_ptr) return(NULL);
	if (offs >= ruletree_ctx.rtree_ruletree_file_size) return(NULL);

	return(((char*)ruletree_ctx.rtree_ruletree_ptr) + offs);
}

/* return a pointer to an object in the rule tree; check that the object
 * exists (by checking the magic number), also check type if a type is provided
*/
void *offset_to_ruletree_object_ptr(ruletree_object_offset_t offs, uint32_t required_type)
{
	ruletree_object_hdr_t	*hdrp = offset_to_raw_ruletree_ptr(offs);

	if (!hdrp) return(NULL);
	if (hdrp->rtree_obj_magic != SB2_RULETREE_MAGIC) return(NULL);
	if (required_type && (required_type != hdrp->rtree_obj_type)) return(NULL);

	return(hdrp);
}

ruletree_object_offset_t append_struct_to_ruletree_file(void *ptr, size_t size, uint32_t type)
{
	ruletree_object_offset_t location = 0;
	ruletree_object_hdr_t	*hdrp = ptr;

	hdrp->rtree_obj_magic = SB2_RULETREE_MAGIC;
	hdrp->rtree_obj_type = type;
	
	if (ruletree_ctx.rtree_ruletree_fd >= 0) {
		location = lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
		if (write(ruletree_ctx.rtree_ruletree_fd, ptr, size) < (int)size) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Failed to append a struct (%d bytes) to the rule tree", size);
		}
		ruletree_ctx.rtree_ruletree_file_size = lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
	}
	return(location);
}


static int open_ruletree_file(int create_if_it_doesnt_exist)
{
	if (!ruletree_ctx.rtree_ruletree_path) return(-1);

	ruletree_ctx.rtree_ruletree_fd = open_nomap_nolog(ruletree_ctx.rtree_ruletree_path,
		O_CLOEXEC | O_RDWR | (create_if_it_doesnt_exist ? O_CREAT : 0),
		S_IRUSR | S_IWUSR);
	if (ruletree_ctx.rtree_ruletree_fd >= 0) {
		ruletree_ctx.rtree_ruletree_file_size = lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "open_ruletree_file => %d", ruletree_ctx.rtree_ruletree_fd);
	return (ruletree_ctx.rtree_ruletree_fd);
}

/* Attach the rule tree = map it to our memoryspace.
 * returns -1 if error, 0 if attached, 1 if created & attached */
int attach_ruletree(const char *ruletree_path,
	int create_if_it_doesnt_exist, int keep_open)
{
	int result = -1;

	SB_LOG(SB_LOGLEVEL_DEBUG, "attach_ruletree(%s)", ruletree_path);

	if (ruletree_path) {
		ruletree_ctx.rtree_ruletree_path = strdup(ruletree_path);
	}

	if (open_ruletree_file(create_if_it_doesnt_exist) < 0) return(-1);

	ruletree_ctx.rtree_ruletree_ptr = mmap(NULL, 16*1024*1024/*length=16MB*/,
		PROT_READ | PROT_WRITE, MAP_SHARED,
		ruletree_ctx.rtree_ruletree_fd, 0);

	if (!ruletree_ctx.rtree_ruletree_ptr) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to mmap() ruletree");
		return(-1);
	}

	if (ruletree_ctx.rtree_ruletree_file_size < sizeof(ruletree_hdr_t)) {
		if (create_if_it_doesnt_exist) {
			/* empty tree - must initialize */
			ruletree_hdr_t	hdr;

			SB_LOG(SB_LOGLEVEL_DEBUG, "empty - initializing");

			lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_SET); 
			result = 1;

			memset(&hdr, 0, sizeof(hdr));
			append_struct_to_ruletree_file(&hdr, sizeof(hdr),
				SB2_RULETREE_OBJECT_TYPE_FILEHDR);
			ruletree_ctx.rtree_ruletree_hdr_p =
				(ruletree_hdr_t*)offset_to_ruletree_object_ptr(0,
					SB2_RULETREE_OBJECT_TYPE_FILEHDR);
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG, "empty - not initializing");
			ruletree_ctx.rtree_ruletree_hdr_p = NULL;
			result = -1;
		}
	} else {
		/* file was not empty, check header */
		ruletree_ctx.rtree_ruletree_hdr_p =
			(ruletree_hdr_t*)offset_to_ruletree_object_ptr(0,
				SB2_RULETREE_OBJECT_TYPE_FILEHDR);

		if (ruletree_ctx.rtree_ruletree_hdr_p) {
			SB_LOG(SB_LOGLEVEL_DEBUG, "header & magic ok");
			result = 0; /* mapped to memory */
		} else {
			SB_LOG(SB_LOGLEVEL_ERROR, "Faulty ruletree header");
			result = -1;
		}
	}
			
	if (keep_open) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "keep_open");
	} else {
		close(ruletree_ctx.rtree_ruletree_fd);
		ruletree_ctx.rtree_ruletree_fd = -1;
		SB_LOG(SB_LOGLEVEL_DEBUG, "rule tree file has been closed.");
	}

	if (result < 0) {
		ruletree_ctx.rtree_ruletree_ptr = NULL;
		ruletree_ctx.rtree_ruletree_hdr_p = NULL;
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "attach_ruletree() => %d", result);
	return(result);
}

/* =================== strings =================== */

const char *offset_to_ruletree_string_ptr(ruletree_object_offset_t offs)
{
	ruletree_string_hdr_t	*strhdr;

	strhdr = offset_to_ruletree_object_ptr(offs,
		SB2_RULETREE_OBJECT_TYPE_STRING);

	if (strhdr) {
		char *str = (char*)strhdr + sizeof(ruletree_string_hdr_t);
		return (str);
	}
	return(NULL);
}

ruletree_object_offset_t append_string_to_ruletree_file(const char *str)
{
	ruletree_string_hdr_t		shdr;
	ruletree_object_offset_t	location = 0;
	int	len = strlen(str);

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	shdr.rtree_str_size = len;
	/* "append_struct_to_ruletree_file" will fill the magic & type */
	location = append_struct_to_ruletree_file(&shdr, sizeof(shdr),
		SB2_RULETREE_OBJECT_TYPE_STRING);
	if (write(ruletree_ctx.rtree_ruletree_fd, str, len+1) < (len + 1)) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to append a string (%d bytes) to the rule tree", len);
	}
	ruletree_ctx.rtree_ruletree_file_size =
		lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
	return(location);
}

/* =================== lists =================== */

ruletree_object_offset_t ruletree_objectlist_create_list(uint32_t size)
{
	ruletree_object_offset_t	location = 0;
	ruletree_objectlist_t		listhdr;
	ruletree_object_offset_t	*a;
	size_t				list_size_in_bytes;

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_create_list(%d) fd=%d",
		size, ruletree_ctx.rtree_ruletree_fd);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	listhdr.rtree_olist_size = size;
	/* "append_struct_to_ruletree_file" will fill the magic & type */
	location = append_struct_to_ruletree_file(&listhdr, sizeof(listhdr),
		SB2_RULETREE_OBJECT_TYPE_OBJECTLIST);
	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_create_list: hdr at %d", location);
	list_size_in_bytes = size * sizeof(ruletree_object_offset_t);
	a = calloc(size, sizeof(ruletree_object_offset_t));
	if (write(ruletree_ctx.rtree_ruletree_fd, a, list_size_in_bytes) < list_size_in_bytes) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to append a list (%d items, %d bytes) to the rule tree", 
			size, list_size_in_bytes);
		location = 0; /* return error */
	}
	ruletree_ctx.rtree_ruletree_file_size =
		lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_create_list: location=%d", location);
	return(location);
}

int ruletree_objectlist_set_item(
	ruletree_object_offset_t list_offs, uint32_t n,
	ruletree_object_offset_t value)
{
	ruletree_objectlist_t		*listhdr;
	ruletree_object_offset_t	*a;

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_set_item(%d,%d,%d)", list_offs, n, value);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (-1);
	listhdr = offset_to_ruletree_object_ptr(list_offs,
		SB2_RULETREE_OBJECT_TYPE_OBJECTLIST);
	if(!listhdr) return(-1);
	if(n >= listhdr->rtree_olist_size) return(-1);

	a = (ruletree_object_offset_t*)((char*)listhdr + sizeof(*listhdr));
	a[n] = value;
	return(1);
}

/* return Nth object offset from a list [N = 0..(size-1)] */
ruletree_object_offset_t ruletree_objectlist_get_item(
	ruletree_object_offset_t list_offs, uint32_t n)
{
	ruletree_objectlist_t		*listhdr;
	ruletree_object_offset_t	*a;

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_get_item(%d,%d)", list_offs, n);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	listhdr = offset_to_ruletree_object_ptr(list_offs,
		SB2_RULETREE_OBJECT_TYPE_OBJECTLIST);
	if(!listhdr) return(0);
	if(n >= listhdr->rtree_olist_size) return(0);

	a = (ruletree_object_offset_t*)((char*)listhdr + sizeof(*listhdr));
	return(a[n]);
}

uint32_t ruletree_objectlist_get_list_size(
	ruletree_object_offset_t list_offs)
{
	ruletree_objectlist_t		*listhdr;

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_objectlist_get_list_size(%d)", list_offs);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	listhdr = offset_to_ruletree_object_ptr(list_offs,
		SB2_RULETREE_OBJECT_TYPE_OBJECTLIST);
	if(!listhdr) return(0);
	return (listhdr->rtree_olist_size);
}

/* =================== catalogs =================== */

ruletree_object_offset_t ruletree_create_catalog_entry(
	const char	*name,
	ruletree_object_offset_t	value_offs)
{
	ruletree_catalog_entry_t	new_entry;
	ruletree_object_offset_t entry_location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	memset(&new_entry, 0, sizeof(new_entry));

	if (name) {
		new_entry.rtree_cat_name_offs = append_string_to_ruletree_file(name);
	}
	new_entry.rtree_cat_value_offs = value_offs;
	new_entry.rtree_cat_next_entry_offs = 0;

	entry_location = append_struct_to_ruletree_file(&new_entry, sizeof(new_entry),
		SB2_RULETREE_OBJECT_TYPE_CATALOG);
	return(entry_location);
}

static ruletree_catalog_entry_t *find_last_catalog_entry(
	ruletree_object_offset_t	catalog_offs)
{
	ruletree_catalog_entry_t	*catp;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (NULL);

	if (!catalog_offs) {
		/* use the root catalog. */
		if (ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog == 0) {
			return(NULL);
		}
		catalog_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
	}

	catp = offset_to_ruletree_object_ptr(catalog_offs,
				SB2_RULETREE_OBJECT_TYPE_CATALOG);

	if (!catp) {
		/* Failed to link it, provided offset is invalid. */
		/* FIXME: Add error message */
		return(NULL);
	}

	while (catp->rtree_cat_next_entry_offs) {
		catp = offset_to_ruletree_object_ptr(
			catp->rtree_cat_next_entry_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG);
	}

	if (!catp) {
		/* Failed to link it, parent catalog is broken. */
		/* FIXME: Add error message */
		return(NULL);
	}
	return(catp);
}

static void link_entry_to_ruletree_catalog(
	ruletree_object_offset_t	catalog_offs,
	ruletree_object_offset_t	new_entry_offs)
{
	ruletree_catalog_entry_t	*prev_entry;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return;
	prev_entry = find_last_catalog_entry(catalog_offs);
	if (!prev_entry && !catalog_offs) {
		/* this will be the first entry in the root catalog. */
		ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog = new_entry_offs;
	}
	if (prev_entry) {
		prev_entry->rtree_cat_next_entry_offs = new_entry_offs;
	}
}

/* add an entry to catalog. if "catalog_offs" is zero, adds to the root catalog.
 * returns location of the new entry.
*/
ruletree_object_offset_t add_entry_to_ruletree_catalog(
	ruletree_object_offset_t	catalog_offs,
	const char	*name,
	ruletree_object_offset_t	value_offs)
{
	ruletree_object_offset_t entry_location;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);

	entry_location = ruletree_create_catalog_entry(name, value_offs);
	link_entry_to_ruletree_catalog(catalog_offs, entry_location);
	return(entry_location);
}

ruletree_object_offset_t ruletree_find_catalog_entry(
	ruletree_object_offset_t	catalog_offs,
	const char	*name)
{
	ruletree_catalog_entry_t	*ep;
	ruletree_object_offset_t entry_location = 0;
	const char	*entry_name;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (NULL);

	if (!catalog_offs) {
		/* use the root catalog. */
		if (ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog == 0) {
			return(0);
		}
		catalog_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
	}

	SB_LOG(SB_LOGLEVEL_NOISE3,
		"ruletree_find_catalog_entry from catalog @ %u)", catalog_offs);
	entry_location = catalog_offs;

	do {
		ep = offset_to_ruletree_object_ptr(entry_location,
					SB2_RULETREE_OBJECT_TYPE_CATALOG);
		if (!ep) return(0);

		entry_name = offset_to_ruletree_string_ptr(ep->rtree_cat_name_offs);
		if (entry_name && !strcmp(name, entry_name)) {
			/* found! */
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"Found entry '%s' @ %u)", name, entry_location);
			return(entry_location);
		}
		entry_location = ep->rtree_cat_next_entry_offs;
	} while (entry_location != 0);

	SB_LOG(SB_LOGLEVEL_NOISE3,
		"'%s' not found", name);
	return(0);
}

ruletree_object_offset_t ruletree_find_or_add_catalog_entry(
	ruletree_object_offset_t	catalog_offs,
	const char	*name)
{
	ruletree_object_offset_t entry_location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	entry_location = ruletree_find_catalog_entry(catalog_offs, name);
	if (!entry_location) {
		/* not found, must add it. */
		entry_location = add_entry_to_ruletree_catalog(catalog_offs,
			name, 0);
	}
	return (entry_location);
}

/* =================== "mode catalogs". OLD CODE. FIXME. =================== */

static ruletree_catalog_entry_t *get_mode_catalog(const char *modename,
	ruletree_object_offset_t *catalog_offs_p)
{
	ruletree_object_offset_t mode_catalog_location;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (NULL);
	mode_catalog_location = ruletree_find_or_add_catalog_entry(0, modename);
	if (catalog_offs_p) *catalog_offs_p = mode_catalog_location;
	if (mode_catalog_location) {
		ruletree_catalog_entry_t *mode_catalog;

		mode_catalog = offset_to_ruletree_object_ptr(
			mode_catalog_location, SB2_RULETREE_OBJECT_TYPE_CATALOG);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"get_mode_catalog(%s) => %u",
			 modename, mode_catalog_location);
		return(mode_catalog);
	}
	return(NULL);
}

ruletree_object_offset_t find_from_mode_catalog(
	const char *modename, const char *objectname)
{
	ruletree_object_offset_t mode_catalog_location;
	ruletree_catalog_entry_t *mode_catalog;
	ruletree_object_offset_t object_location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);

	mode_catalog = get_mode_catalog(modename, &mode_catalog_location);

	if (mode_catalog) {
		ruletree_object_offset_t object_catalog_entry_location;
		ruletree_catalog_entry_t *object_catalog_entry;

		object_catalog_entry_location = ruletree_find_catalog_entry(
			mode_catalog->rtree_cat_value_offs, objectname);

		object_catalog_entry = offset_to_ruletree_object_ptr(
			object_catalog_entry_location, SB2_RULETREE_OBJECT_TYPE_CATALOG);
		if (object_catalog_entry) {
			object_location = object_catalog_entry->rtree_cat_value_offs;
		}
	}

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"find_from_mode_catalog(%s,%s) => %u",
		 modename, objectname, object_location);

	return(object_location);
}

static void add_object_to_mode_catalog(
	const char *modename, const char *newobjectname, ruletree_object_offset_t loc)
{
	ruletree_object_offset_t mode_catalog_location;
	ruletree_catalog_entry_t *mode_catalog;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return;

	mode_catalog = get_mode_catalog(modename, &mode_catalog_location);
	if (mode_catalog) {
		ruletree_object_offset_t new_object_entry_location;

		new_object_entry_location = ruletree_create_catalog_entry(
			newobjectname, loc);

		if (mode_catalog->rtree_cat_value_offs == 0) {
			/* first entry */
			mode_catalog->rtree_cat_value_offs = new_object_entry_location;
		} else {
			/* the catalog exist, add another entry to it. */
			link_entry_to_ruletree_catalog(
				mode_catalog->rtree_cat_value_offs,
				new_object_entry_location);
		}
	} else {
		/* FIXME: failed to get catalog location - add error message */
	}
	
}

/* FIXME: sensible name needed */
int set_ruletree_fsrules(const char *modename, const char *rules_name, int loc)
{
	int result = 0; /* boolean; default result is "false" */
	
	if (ruletree_ctx.rtree_ruletree_hdr_p) {
		add_object_to_mode_catalog(modename, rules_name, loc);
		result = 1;
	}
	
	return(result);
}

/* =================== FS rules =================== */

ruletree_fsrule_t *offset_to_ruletree_fsrule_ptr(int loc)
{
	ruletree_fsrule_t	*rp;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (NULL);
	rp = (ruletree_fsrule_t*)offset_to_ruletree_object_ptr(loc,
		SB2_RULETREE_OBJECT_TYPE_FSRULE);
	if (rp) {
		if (rp->rtree_fsr_objhdr.rtree_obj_magic != SB2_RULETREE_MAGIC) rp = NULL;
	}
	return(rp);
}

/* =================== map "standard" ruletree to memory, if not yet mapped =================== */

/* ensure that the rule tree has been mapped. */
int ruletree_to_memory(void)
{
        int attach_result = -1;

	if (ruletree_ctx.rtree_ruletree_path) {
                SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_to_memory: already done");
		return(0); /* return if already mapped */
	}

        if (sbox_session_dir) {
                char *rule_tree_path = NULL;

                /* map the rule tree to memory: */
                asprintf(&rule_tree_path, "%s/RuleTree.bin", sbox_session_dir);
                attach_result = attach_ruletree(rule_tree_path,
                        0/*create if needed*/, 0/*keep open*/);
                SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_to_memory: attach(%s) = %d",
			rule_tree_path, attach_result);
                free(rule_tree_path);
        } else {
                SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_to_memory: no session dir");
	}
        return (attach_result);
}

