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
	void		*rtree_ruletree_ptr;
	ruletree_hdr_t	*rtree_ruletree_hdr_p;
} ruletree_ctx = { NULL, -1, 0, NULL };

/* =================== Rule tree primitives. =================== */

size_t ruletree_get_file_size(void)
{
	if (ruletree_ctx.rtree_ruletree_hdr_p) return (ruletree_ctx.rtree_ruletree_hdr_p->rtree_file_size);
	return(0);
}

/* return a pointer to the rule tree, without checking the contents */
static void *offset_to_raw_ruletree_ptr(ruletree_object_offset_t offs)
{
	if (!ruletree_ctx.rtree_ruletree_ptr) return(NULL);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return(NULL);
	if (offs >= ruletree_ctx.rtree_ruletree_hdr_p->rtree_file_size) return(NULL);

	return(((char*)ruletree_ctx.rtree_ruletree_ptr) + offs);
}

/* return a pointer to an object in the rule tree; check that the object
 * exists (by checking the magic number), also check type if a type is provided
*/
void *offset_to_ruletree_object_ptr(ruletree_object_offset_t offs, uint32_t required_type)
{
	ruletree_object_hdr_t	*hdrp = offset_to_raw_ruletree_ptr(offs);

	if (!hdrp) {
		SB_LOG(SB_LOGLEVEL_NOISE3, "%s: no hdrp @%u", __func__, offs);
		return(NULL);
	}
	if (hdrp->rtree_obj_magic != SB2_RULETREE_MAGIC) {
		SB_LOG(SB_LOGLEVEL_NOISE3, "%s: wrong magic 0x%X", __func__,
			hdrp->rtree_obj_magic);
		return(NULL);
	}
	if (required_type && (required_type != hdrp->rtree_obj_type)) {
		SB_LOG(SB_LOGLEVEL_NOISE3, "%s: wrong type (req=0x%X, was 0x%X)",
			__func__, required_type, hdrp->rtree_obj_type);
		return(NULL);
	}

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
		if (ruletree_ctx.rtree_ruletree_hdr_p) 
			ruletree_ctx.rtree_ruletree_hdr_p->rtree_file_size =
				lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END); 
	}
	return(location);
}


static int open_ruletree_file(int create_if_it_doesnt_exist)
{
	if (!ruletree_ctx.rtree_ruletree_path) return(-1);

	ruletree_ctx.rtree_ruletree_fd = open_nomap_nolog(ruletree_ctx.rtree_ruletree_path,
		O_CLOEXEC | O_RDWR | (create_if_it_doesnt_exist ? O_CREAT : 0),
		S_IRUSR | S_IWUSR);

	SB_LOG(SB_LOGLEVEL_DEBUG, "open_ruletree_file => %d", ruletree_ctx.rtree_ruletree_fd);
	return (ruletree_ctx.rtree_ruletree_fd);
}

static int mmap_ruletree(ruletree_hdr_t *hdr)
{
	ruletree_ctx.rtree_ruletree_ptr = mmap(
		(void*)hdr->rtree_min_mmap_addr, hdr->rtree_max_size,
		PROT_READ | PROT_WRITE, MAP_SHARED,
		ruletree_ctx.rtree_ruletree_fd, 0);

	if (!ruletree_ctx.rtree_ruletree_ptr) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to mmap() ruletree");
		return(-1);
	}

	/* use the force, otherwise offset_to_ruletree_object_ptr()
	 * fails */
	ruletree_ctx.rtree_ruletree_hdr_p =
		(ruletree_hdr_t*)ruletree_ctx.rtree_ruletree_ptr;
	/* now do the same without force. */
	ruletree_ctx.rtree_ruletree_hdr_p =
		(ruletree_hdr_t*)offset_to_ruletree_object_ptr(0,
			SB2_RULETREE_OBJECT_TYPE_FILEHDR);

	if (ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree mmap'ed ok");
		return(0);
	}
	SB_LOG(SB_LOGLEVEL_ERROR, "Faulty ruletree header");
	return(-1);
}

/* For the server:
 * create and attach a rule tree file, leaves it open for writing */
int create_ruletree_file(const char *ruletree_path,
	uint32_t max_size, uint32_t min_mmap_addr, int min_client_socket_fd)
{
	ruletree_hdr_t	hdr;

	if (!ruletree_path) return(-1);

	ruletree_ctx.rtree_ruletree_path = strdup(ruletree_path);

	if (open_ruletree_file(1/*create_if_it_doesnt_exist*/) < 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "create_ruletree_file: open() failed");
		return(-1);
	}

	if (lseek(ruletree_ctx.rtree_ruletree_fd, 0, SEEK_END) != 0) { 
		SB_LOG(SB_LOGLEVEL_DEBUG, "create_ruletree_file: file is not empty");
		return(-1);
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "create_ruletree_file - initializing rule tree db");

	memset(&hdr, 0, sizeof(hdr));
	hdr.rtree_version = RULE_TREE_VERSION;
	hdr.rtree_file_size = sizeof(hdr);
	hdr.rtree_max_size = max_size;
	hdr.rtree_min_mmap_addr = min_mmap_addr;
	hdr.rtree_min_client_socket_fd = min_client_socket_fd;
	append_struct_to_ruletree_file(&hdr, sizeof(hdr),
		SB2_RULETREE_OBJECT_TYPE_FILEHDR);

	if (mmap_ruletree(&hdr) < 0) return(-1);
	
	return(0);
}

int ruletree_get_min_client_socket_fd(void)
{
	if (ruletree_ctx.rtree_ruletree_hdr_p)
		return(ruletree_ctx.rtree_ruletree_hdr_p->rtree_min_client_socket_fd);
	return(0);
}

/* For clients:
 * Attach the rule tree = map it to our memoryspace.
 * returns -1 if error, 0 if attached
*/
int attach_ruletree(const char *ruletree_path, int keep_open)
{
	ruletree_hdr_t	hdr;

	SB_LOG(SB_LOGLEVEL_DEBUG, "attach_ruletree(%s)", ruletree_path);

	if (!ruletree_ctx.rtree_ruletree_path && ruletree_path) {
		ruletree_ctx.rtree_ruletree_path = strdup(ruletree_path);
	} else if (!ruletree_path) return(-1);

	if (open_ruletree_file(0/*create_if_it_doesnt_exist*/) < 0) return(-1);

	if (read(ruletree_ctx.rtree_ruletree_fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Illegal ruletree file size or format");
		return(-1);
	}
	if (hdr.rtree_version != RULE_TREE_VERSION) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Fatal: ruletree version mismatch: Got %d, expected %d",
			ruletree_ctx.rtree_ruletree_hdr_p->rtree_version,
			RULE_TREE_VERSION);
		exit(44);
	}

	if (mmap_ruletree(&hdr) < 0) return(-1);

	if (!keep_open) {
		close(ruletree_ctx.rtree_ruletree_fd);
		ruletree_ctx.rtree_ruletree_fd = -1;
		SB_LOG(SB_LOGLEVEL_DEBUG, "rule tree file has been closed.");
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "attach_ruletree() => OK");
	return(0);
}

/* =================== ints and booleans =================== */

static uint32_t *ruletree_get_pointer_to_uint32_or_boolean(
	ruletree_object_offset_t offs, int type)
{
	ruletree_uint32_t	*hdrp;

	hdrp = offset_to_ruletree_object_ptr(offs, type);
	if (hdrp) {
		return(&hdrp->rtree_uint32);
	}
	return(NULL);
}

uint32_t *ruletree_get_pointer_to_uint32(ruletree_object_offset_t offs)
{
	return(ruletree_get_pointer_to_uint32_or_boolean(offs,
		SB2_RULETREE_OBJECT_TYPE_UINT32));
}

uint32_t *ruletree_get_pointer_to_boolean(ruletree_object_offset_t offs)
{
	return(ruletree_get_pointer_to_uint32_or_boolean(offs,
		SB2_RULETREE_OBJECT_TYPE_BOOLEAN));
}

static ruletree_object_offset_t append_uint32_or_boolean_to_ruletree_file(
	uint32_t initial_value,  int type)
{
	ruletree_uint32_t		uihdr;
	ruletree_object_offset_t	location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	uihdr.rtree_uint32 = initial_value;
	/* "append_struct_to_ruletree_file" will fill the magic & type */
	location = append_struct_to_ruletree_file(&uihdr, sizeof(uihdr),
		type);
	return(location);
}

ruletree_object_offset_t append_uint32_to_ruletree_file(uint32_t initial_value)
{
	return(append_uint32_or_boolean_to_ruletree_file(initial_value,
		SB2_RULETREE_OBJECT_TYPE_UINT32));
}

ruletree_object_offset_t append_boolean_to_ruletree_file(uint32_t initial_value)
{
	return(append_uint32_or_boolean_to_ruletree_file(initial_value,
		SB2_RULETREE_OBJECT_TYPE_BOOLEAN));
}

/* =================== strings =================== */

const char *offset_to_ruletree_string_ptr(ruletree_object_offset_t offs,
	uint32_t *lenp)
{
	ruletree_string_hdr_t	*strhdr;

	strhdr = offset_to_ruletree_object_ptr(offs,
		SB2_RULETREE_OBJECT_TYPE_STRING);

	if (strhdr) {
		char *str = (char*)strhdr + sizeof(ruletree_string_hdr_t);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"offset_to_ruletree_string_ptr returns '%s' (%u)",
			str, strhdr->rtree_str_size);
		if (lenp) {
			*lenp = strhdr->rtree_str_size;
		}
		return (str);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"offset_to_ruletree_string_ptr returns NULL");
	return(NULL);
}

ruletree_object_offset_t append_string_to_ruletree_file(const char *str)
{
	ruletree_string_hdr_t		shdr;
	ruletree_object_offset_t	location = 0;
	int	len;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);
	if (!str) return(0);

	len = strlen(str);
	shdr.rtree_str_size = len;
	/* "append_struct_to_ruletree_file" will fill the magic & type */
	location = append_struct_to_ruletree_file(&shdr, sizeof(shdr),
		SB2_RULETREE_OBJECT_TYPE_STRING);
	if (write(ruletree_ctx.rtree_ruletree_fd, str, len+1) < (len + 1)) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to append a string (%d bytes) to the rule tree", len);
	}
	if (ruletree_ctx.rtree_ruletree_hdr_p)
		ruletree_ctx.rtree_ruletree_hdr_p->rtree_file_size =
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
	if (ruletree_ctx.rtree_ruletree_hdr_p)
		ruletree_ctx.rtree_ruletree_hdr_p->rtree_file_size =
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

	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_objectlist_set_item(%d,%d,%d)", list_offs, n, value);
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

	SB_LOG(SB_LOGLEVEL_NOISE2, "ruletree_objectlist_get_item(%d,%d)", list_offs, n);
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

	SB_LOG(SB_LOGLEVEL_NOISE, "ruletree_objectlist_get_list_size(%d)", list_offs);
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	listhdr = offset_to_ruletree_object_ptr(list_offs,
		SB2_RULETREE_OBJECT_TYPE_OBJECTLIST);
	if(!listhdr) return(0);
	return (listhdr->rtree_olist_size);
}

/* =================== catalogs =================== */

static ruletree_object_offset_t ruletree_create_catalog_entry(
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
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"find_last_catalog_entry: catalog @%d",
		(int)catalog_offs);

	catp = offset_to_ruletree_object_ptr(catalog_offs,
				SB2_RULETREE_OBJECT_TYPE_CATALOG);

	if (!catp) {
		/* Failed to link it, provided offset is invalid. */
		/* FIXME: Add error message */
		return(NULL);
	}

	while (catp->rtree_cat_next_entry_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"find_last_catalog_entry: move to @%d",
			(int)catp->rtree_cat_next_entry_offs);
		catp = offset_to_ruletree_object_ptr(
			catp->rtree_cat_next_entry_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG);
	}

	if (!catp) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"find_last_catalog_entry: Error: parent catalog is broken, catalog @%d",
			(int)catalog_offs);
		return(NULL);
	}
	return(catp);
}

static void link_entry_to_ruletree_catalog(
	ruletree_object_offset_t	catalog_offs,
	ruletree_object_offset_t	new_entry_offs)
{
	ruletree_catalog_entry_t	*prev_entry;

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"link_entry_to_ruletree_catalog");
	if (!ruletree_ctx.rtree_ruletree_hdr_p) return;
	prev_entry = find_last_catalog_entry(catalog_offs);
	if (!prev_entry && !catalog_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"link_entry_to_ruletree_catalog: "
			"this will be the first entry in the root catalog.");
		ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog = new_entry_offs;
	}
	if (prev_entry) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"link_entry_to_ruletree_catalog: linking");
		prev_entry->rtree_cat_next_entry_offs = new_entry_offs;
	}
}

/* add an entry to catalog. if "catalog_offs" is zero, adds to the root catalog.
 * returns location of the new entry.
*/
static ruletree_object_offset_t add_entry_to_ruletree_catalog(
	ruletree_object_offset_t	catalog_offs,
	const char	*name,
	ruletree_object_offset_t	value_offs)
{
	ruletree_object_offset_t entry_location;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"add_entry_to_ruletree_catalog: catalog @%d, name=%s",
		(int)catalog_offs, name);
	entry_location = ruletree_create_catalog_entry(name, value_offs);
	link_entry_to_ruletree_catalog(catalog_offs, entry_location);
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"add_entry_to_ruletree_catalog: new entry @%d",
		(int)entry_location);
	return(entry_location);
}

static ruletree_object_offset_t ruletree_find_catalog_entry(
	ruletree_object_offset_t	catalog_offs,
	const char	*name)
{
	ruletree_catalog_entry_t	*ep;
	ruletree_object_offset_t entry_location = 0;
	const char	*entry_name;
	int		name_len;

	if (!name) return(0);

	if (!ruletree_ctx.rtree_ruletree_hdr_p) ruletree_to_memory();

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
	name_len = strlen(name);

	do {
		uint32_t	entry_name_len;

		ep = offset_to_ruletree_object_ptr(entry_location,
					SB2_RULETREE_OBJECT_TYPE_CATALOG);
		if (!ep) return(0);

		entry_name = offset_to_ruletree_string_ptr(ep->rtree_cat_name_offs,
			&entry_name_len);
		if (entry_name && 
		    (name_len == entry_name_len) &&
		    !strcmp(name, entry_name)) {
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

/* --- public routines --- */

/* get a value for "object_name" from catalog "catalog_name".
 * returns 0 if:
 *  - "name" does not exist
 *  - there is no catalog at "catalog_offs"
 *  - or any other error.
*/
ruletree_object_offset_t ruletree_catalog_get(
	const char *catalog_name,
	const char *object_name)
{
	ruletree_object_offset_t	root_catalog_offs = 0;
	ruletree_object_offset_t	catalog_entry_in_root_catalog_offs = 0;
	ruletree_object_offset_t	subcatalog_start_offs = 0;
	ruletree_catalog_entry_t	*catalog_cat_entry = NULL;
	ruletree_catalog_entry_t	*object_cat_entry = NULL;
	ruletree_object_offset_t	object_offs = 0;

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get(catalog=%s,object_name=%s)",
		catalog_name, object_name);

	if (!ruletree_ctx.rtree_ruletree_hdr_p) ruletree_to_memory();

	if (!ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed, no rule tree");
		return (0);
	}

	/* find the catalog from the root catalog. */
	root_catalog_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
	if (!root_catalog_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed, no root catalog.");
			return(0);
	}
	catalog_entry_in_root_catalog_offs = ruletree_find_catalog_entry(
		0/*root catalog*/, catalog_name);
	if (!catalog_entry_in_root_catalog_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed, no subcatalog.");
		return(0);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get: catalog @%d",
		(int)catalog_entry_in_root_catalog_offs);

	catalog_cat_entry = offset_to_ruletree_object_ptr(catalog_entry_in_root_catalog_offs,
		SB2_RULETREE_OBJECT_TYPE_CATALOG);
	if (!catalog_cat_entry) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed: not a catalog entry @%d",
			(int)catalog_entry_in_root_catalog_offs);
		return(0);
	}

	subcatalog_start_offs = catalog_cat_entry->rtree_cat_value_offs;
	if (!subcatalog_start_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed: nothing in the subcatalog");
		return(0);
	}

	object_offs = ruletree_find_catalog_entry(subcatalog_start_offs, object_name);
	if (!object_offs) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed: not found from the subcatalog");
		return(0);
	}
	object_cat_entry = offset_to_ruletree_object_ptr(object_offs,
		SB2_RULETREE_OBJECT_TYPE_CATALOG);
	if (!object_cat_entry) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get: Failed: not a catalog entry @%d",
				(int)object_offs);
		return(0);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get: Found value=@%d",
			(int)object_cat_entry->rtree_cat_value_offs);
	return(object_cat_entry->rtree_cat_value_offs);
}

const char *ruletree_catalog_get_string(
	const char *catalog_name,
	const char *object_name)
{
	ruletree_object_offset_t offs;

	if (!ruletree_ctx.rtree_ruletree_path) ruletree_to_memory();

	offs = ruletree_catalog_get(catalog_name, object_name);
	if(offs) {
		const char *str = offset_to_ruletree_string_ptr(offs, NULL);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get_string: '%s'", str);
		return(str);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get_string: NULL");
	return(NULL);
}

uint32_t *ruletree_catalog_get_uint32_ptr(
        const char *catalog_name, const char *object_name)
{
	ruletree_object_offset_t offs;

	if (!ruletree_ctx.rtree_ruletree_path) ruletree_to_memory();

	offs = ruletree_catalog_get(catalog_name, object_name);
	if(offs) {
		uint32_t *uip = ruletree_get_pointer_to_uint32(offs);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get_uint32_ptr(%s,%s): 0x%x",
			catalog_name, object_name, (int)uip);
		return(uip);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get_uint32_ptr(%s,%s): NULL",
			catalog_name, object_name);
	return(NULL);
}

uint32_t *ruletree_catalog_get_boolean_ptr(
        const char *catalog_name, const char *object_name)
{
	ruletree_object_offset_t offs;

	if (!ruletree_ctx.rtree_ruletree_path) ruletree_to_memory();

	offs = ruletree_catalog_get(catalog_name, object_name);
	if(offs) {
		uint32_t *uip = ruletree_get_pointer_to_boolean(offs);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get_boolean_ptr(%s,%s): 0x%x",
			catalog_name, object_name, (int)uip);
		return(uip);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_get_boolean_ptr(%s,%s): NULL",
			catalog_name, object_name);
	return(NULL);
}

/* set a value for "name" in a catalog.
 * returns positive if OK, or 0 if failed:
 * side effects: creates a new catalog and/or
 * entry if needed.
*/
int ruletree_catalog_set(
	const char	*catalog_name,
	const char	*object_name,
	ruletree_object_offset_t value_offset)
{
	ruletree_object_offset_t	root_catalog_offs = 0;
	ruletree_object_offset_t	catalog_entry_in_root_catalog_offs = 0;
	ruletree_object_offset_t	subcatalog_start_offs = 0;
	ruletree_catalog_entry_t	*catalog_cat_entry = NULL;
	ruletree_catalog_entry_t	*object_cat_entry = NULL;
	ruletree_object_offset_t	object_offs = 0;

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_catalog_set(catalog=%s,object_name=%s,object_offset=%d)",
		catalog_name, object_name, (int)value_offset);

	if (!ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_set: Failed, no rule tree");
		return (0);
	}

	/* find the catalog from the root catalog. */
	root_catalog_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
	if (!root_catalog_offs) {
		/* no root catalog, try to create it first. */
		if (ruletree_ctx.rtree_ruletree_fd < 0) {
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"ruletree_catalog_set: no root catalog, can't create it.");
			return(0);
		}
		/* create the catalog. */
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_set: create root catalog, add %s",
				catalog_name);

		catalog_entry_in_root_catalog_offs = add_entry_to_ruletree_catalog(
			0/*root catalog*/, catalog_name, 0);
	} else {
		catalog_entry_in_root_catalog_offs = ruletree_find_catalog_entry(
			0/*root catalog*/, catalog_name);
		if (!catalog_entry_in_root_catalog_offs) {
			if (ruletree_ctx.rtree_ruletree_fd < 0) {
				SB_LOG(SB_LOGLEVEL_NOISE2,
					"ruletree_catalog_set: no catalog, can't create it.");
				return(0);
			}
			catalog_entry_in_root_catalog_offs = add_entry_to_ruletree_catalog(
				0/*root catalog*/, catalog_name, 0);
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"ruletree_catalog_set: catalog @%d",
				(int)catalog_entry_in_root_catalog_offs);
		}
	}
	catalog_cat_entry = offset_to_ruletree_object_ptr(catalog_entry_in_root_catalog_offs,
		SB2_RULETREE_OBJECT_TYPE_CATALOG);
	if (!catalog_cat_entry) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_catalog_set: Error: not a catalog entry @%d",
			(int)catalog_entry_in_root_catalog_offs);
		return(0);
	}
	subcatalog_start_offs = catalog_cat_entry->rtree_cat_value_offs;
	if (subcatalog_start_offs) {
		/* the catalog already has something */
		object_offs = ruletree_find_catalog_entry(subcatalog_start_offs, object_name);
		if (!object_offs) {
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"ruletree_catalog_set: add %s=%d to catalog @%d",
				object_name, (int)value_offset, (int)subcatalog_start_offs);
			object_offs = add_entry_to_ruletree_catalog(
				subcatalog_start_offs, object_name, (int)value_offset);
			if (!object_offs) {
				SB_LOG(SB_LOGLEVEL_NOISE2,
					"ruletree_catalog_set: failed to create %s", object_name);
				return(0);
			}
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"ruletree_catalog_set: set %s=%d", object_name, (int)value_offset);
		}
		object_cat_entry = offset_to_ruletree_object_ptr(object_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG);
		if (!object_cat_entry) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"ruletree_catalog_set: Error: not a catalog entry @%d",
				(int)object_offs);
			return(0);
		}
		object_cat_entry->rtree_cat_value_offs = value_offset;
	} else {
		/* nothing in the subcatalog, add object */
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_set: add first entry %s=%d", object_name, (int)value_offset);
		object_offs = ruletree_create_catalog_entry(object_name, value_offset);
		catalog_cat_entry->rtree_cat_value_offs = object_offs;
	}
	return(1);
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

/* =================== Exec rules =================== */

ruletree_exec_preprocessing_rule_t *offset_to_exec_preprocessing_rule_ptr(int loc)
{
	ruletree_exec_preprocessing_rule_t	*rp;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (NULL);
	rp = (ruletree_exec_preprocessing_rule_t*)offset_to_ruletree_object_ptr(loc,
		SB2_RULETREE_OBJECT_TYPE_EXEC_PP_RULE);
	if (rp) {
		if (rp->rtree_xpr_objhdr.rtree_obj_magic != SB2_RULETREE_MAGIC) rp = NULL;
	}
	return(rp);
}

/* =================== map "standard" ruletree to memory, if not yet mapped =================== */

/* ensure that the rule tree has been mapped. */
/* FIXME: CHECK: maybe this could be a private routine. */
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
                if (asprintf(&rule_tree_path, "%s/RuleTree.bin", sbox_session_dir) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create file name for rule tree");
		} else {
			attach_result = attach_ruletree(rule_tree_path,
				0/*keep open*/);
			SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_to_memory: attach(%s) = %d",
				rule_tree_path, attach_result);
			free(rule_tree_path);
		}
        } else {
                SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_to_memory: no session dir");
	}
        return (attach_result);
}

