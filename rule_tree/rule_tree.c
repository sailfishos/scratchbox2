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
		(void*)(uintptr_t)(hdr->rtree_min_mmap_addr), hdr->rtree_max_size,
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
	uint32_t max_size, uint64_t min_mmap_addr, int min_client_socket_fd)
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
	ssize_t				wr_result;

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
	wr_result = write(ruletree_ctx.rtree_ruletree_fd, a, list_size_in_bytes);
	if ((wr_result == -1) || ((size_t)wr_result < list_size_in_bytes)) {
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

/* =================== binary trees =================== */

static ruletree_object_offset_t ruletree_create_bintree_entry(
	uint64_t	key1,
	uint64_t	key2,
	ruletree_object_offset_t	value_offs)
{
	ruletree_bintree_t	new_entry;
	ruletree_object_offset_t entry_location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	memset(&new_entry, 0, sizeof(new_entry));

	new_entry.rtree_bt_key1 = key1;
	new_entry.rtree_bt_key2 = key2;
	new_entry.rtree_bt_value = value_offs;

	entry_location = append_struct_to_ruletree_file(&new_entry, sizeof(new_entry),
		SB2_RULETREE_OBJECT_TYPE_BINTREE);
	SB_LOG(SB_LOGLEVEL_NOISE,
		"ruletree_create_bintree_entry: @%d",
		(int)entry_location);
	return(entry_location);
}

static ruletree_object_offset_t ruletree_find_bintree_entry(
	uint64_t	key1,
	uint64_t	key2,
	ruletree_object_offset_t	root_offs,
	ruletree_object_offset_t	*last_visited_node,
	int				*last_result)
{
	ruletree_object_offset_t	node_offs;
	ruletree_object_offset_t	last_compared_node = 0;
	ruletree_object_offset_t	last_direction = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (!root_offs) return(0);

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_find_bintree_entry: @%d : key1=0x%llX key2=%lld",
		(int)root_offs, (long long)key1, (long long)key2);

	node_offs = root_offs;

	while(node_offs) {
		ruletree_bintree_t	*bintrp;

		bintrp = offset_to_ruletree_object_ptr(node_offs,
				SB2_RULETREE_OBJECT_TYPE_BINTREE);
		if (!bintrp) {
			node_offs = 0;
			break;
		}
		SB_LOG(SB_LOGLEVEL_NOISE3,
			"ruletree_find_bintree_entry: check @%d",
			node_offs);
		
		if ((bintrp->rtree_bt_key1 == key1) &&
		    (bintrp->rtree_bt_key2 == key2)) {
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"ruletree_find_bintree_entry: FOUND");
			last_direction = 0;
			return(node_offs);
		}
		last_compared_node = node_offs;
		if ((key1 < bintrp->rtree_bt_key1) ||
		    ((key1 == bintrp->rtree_bt_key1) &&
		     (key2 <  bintrp->rtree_bt_key2))) {
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"ruletree_find_bintree_entry: less");
			last_direction = -1;
			node_offs = bintrp->rtree_bt_link_less;
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"ruletree_find_bintree_entry: more");
			last_direction = +1;
			node_offs = bintrp->rtree_bt_link_more;
		}
	}
	SB_LOG(SB_LOGLEVEL_NOISE3,
		"ruletree_find_bintree_entry: Not found.");
	if(last_visited_node) *last_visited_node = last_compared_node;
	if(last_result) *last_result = last_direction;
	return(node_offs);
}

/* returns 0 or offset to new root. */
static ruletree_object_offset_t ruletree_add_to_bintree_entry(
	ruletree_object_offset_t value_node_offs,
	uint64_t key1,
	uint64_t key2,
	ruletree_object_offset_t last_compared_node,
	ruletree_object_offset_t last_direction)
{
	ruletree_bintree_t	*parent_bintrp;
	ruletree_object_offset_t new_bintree_node_offs;

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"ruletree_add_to_bintree_entry: @%d",
		(int)value_node_offs);

	if (last_compared_node == 0) {
		/* no root - add first node. */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_add_to_bintree_entry: First node");
		new_bintree_node_offs = ruletree_create_bintree_entry(
			key1, key2, value_node_offs);
		return(new_bintree_node_offs);
	}

	parent_bintrp = offset_to_ruletree_object_ptr(last_compared_node,
				SB2_RULETREE_OBJECT_TYPE_BINTREE);

	if (!parent_bintrp) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_add_to_bintree_entry: invalid parent");
		return(0);
	}

	new_bintree_node_offs = ruletree_create_bintree_entry(
		key1, key2, value_node_offs);
	switch (last_direction) {
	case -1:
		if ((key1 < parent_bintrp->rtree_bt_key1) ||
		    ((key1 == parent_bintrp->rtree_bt_key1) &&
		     (key2 <  parent_bintrp->rtree_bt_key2))) {
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"ruletree_add_to_bintree_entry: less");
			if (parent_bintrp->rtree_bt_link_less) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"ruletree_add_to_bintree_entry: less already exits!");
			} else {
				parent_bintrp->rtree_bt_link_less = new_bintree_node_offs;
			}
		} else {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"ruletree_add_to_bintree_entry: invalid direction (-1)");
		}
		break;
	case 1:
		if ((key1 < parent_bintrp->rtree_bt_key1) ||
		    ((key1 == parent_bintrp->rtree_bt_key1) &&
		     (key2 <  parent_bintrp->rtree_bt_key2))) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"ruletree_add_to_bintree_entry: invalid direction (+1)");
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE3,
				"ruletree_add_to_bintree_entry: more");
			if (parent_bintrp->rtree_bt_link_more) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"ruletree_add_to_bintree_entry: more already exits!");
			} else {
				parent_bintrp->rtree_bt_link_more = new_bintree_node_offs;
			}
		}
		break;
	default:
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"ruletree_add_to_bintree_entry: invalid direction(%d)!",
			last_direction);
		break;
	}
	return(0);
}

/* =================== file/inode status simulation structures =================== */

static ruletree_object_offset_t ruletree_create_inodestat(
	inodesimu_t	*istat_struct)
{
	ruletree_inodestat_t	new_entry;
	ruletree_object_offset_t entry_location = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) return (0);
	if (ruletree_ctx.rtree_ruletree_fd < 0) return(0);

	memset(&new_entry, 0, sizeof(new_entry));

	new_entry.rtree_inode_simu = *istat_struct;

	entry_location = append_struct_to_ruletree_file(&new_entry, sizeof(new_entry),
		SB2_RULETREE_OBJECT_TYPE_INODESTAT);
	return(entry_location);
}

/* Inode number is the primary key to the bintree
 * (device number is the secondary key), but
 * sometimes inode allocation may be done somewhat
 * sequentially. For a slightly better balancing 
 * of the tree, reverse some bits of the key:
 * This algorithm takes the lowest 8 bits, reverses
 * them, and the result will be in bits 32..39 or
 * "k" below (rest of bits in "k" are copies of the
 * original bits or zero, those cause no harm here)
 *
 * For detailed explanation, see 
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64Bits
*/
static uint64_t ino_to_key(uint64_t ino)
{
	uint64_t	k;

	k = ((((ino & 0xFF) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL) << 24;
	return(k ^ ino);
}

static ruletree_object_offset_t	inodestats_bintree_root = 0;

/* in: "handle" contains the keys
 * out: istat_struct has been filled, if a matching node was found.
 *	in any case, "handle" has been updated so that 
 *      ruletree_set_inodestat() can be called later to add/update
 *	a istat_struct.
 * returns 0 if OK, negative if not found. */
int ruletree_find_inodestat(
	ruletree_inodestat_handle_t	*handle,
	inodesimu_t			*istat_struct)
{
	ruletree_bintree_t	*bintrp;
	ruletree_inodestat_t	*fsptr;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"ruletree_find_inodestat (dev=%lld,ino=%lld,key=%llX)",
			(long long)handle->rfh_dev,
			(long long)handle->rfh_ino,
			ino_to_key(handle->rfh_ino));
	handle->rfh_last_visited_node = 0;
	handle->rfh_last_result = 0;

	if (!ruletree_ctx.rtree_ruletree_path) ruletree_to_memory();

	if (!inodestats_bintree_root) {
		inodestats_bintree_root = ruletree_catalog_get(
			"vperm", "inodestats");
		if (!inodestats_bintree_root) return(-1);
	}
	handle->rfh_offs = ruletree_find_bintree_entry(
		ino_to_key(handle->rfh_ino), handle->rfh_dev,
		inodestats_bintree_root, &handle->rfh_last_visited_node,
		&handle->rfh_last_result);
	if (!handle->rfh_offs) return(-1);
		
	bintrp = offset_to_ruletree_object_ptr(handle->rfh_offs,
			SB2_RULETREE_OBJECT_TYPE_BINTREE);
	if (!bintrp) return(-1);
	fsptr = offset_to_ruletree_object_ptr(bintrp->rtree_bt_value,
			SB2_RULETREE_OBJECT_TYPE_INODESTAT);
	if (!fsptr) return(-1);

	*istat_struct = fsptr->rtree_inode_simu;

	return(0);
}

/* set/add a inodestat structure to the binary tree.
 * ruletree_find_inodestat() must be called beforehand to 
 * fill "handle" (unless adding the very first node)
 *
 * returns 0 or offset to new binary tree root. */
ruletree_object_offset_t ruletree_set_inodestat(
	ruletree_inodestat_handle_t	*handle,
	inodesimu_t			*istat_struct)
{
	SB_LOG(SB_LOGLEVEL_NOISE,
		"ruletree_set_inodestat (dev=%lld,ino=%lld,key=%llX))",
			(long long)handle->rfh_dev,
			(long long)handle->rfh_ino,
			ino_to_key(handle->rfh_ino));
	if (handle->rfh_offs) {
		/* Node is already in the tree. Update it */
		ruletree_inodestat_t	*fsptr;
		ruletree_bintree_t	*bintrp;

		bintrp = offset_to_ruletree_object_ptr(handle->rfh_offs,
				SB2_RULETREE_OBJECT_TYPE_BINTREE);
		if (!bintrp) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"ruletree_set_inodestat: Internal error: Invalid handle");
			return(0);
		}
		fsptr = offset_to_ruletree_object_ptr(bintrp->rtree_bt_value,
				SB2_RULETREE_OBJECT_TYPE_INODESTAT);
		if (!fsptr) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"ruletree_set_inodestat: Internal error: Invalid bintree");
			return(0);
		}
		SB_LOG(SB_LOGLEVEL_NOISE,
			"ruletree_set_inodestat: set info");
		fsptr->rtree_inode_simu = *istat_struct;
		return(0);
	} else {
		/* Add to the tree. */
		ruletree_object_offset_t	bt_root;

		SB_LOG(SB_LOGLEVEL_NOISE,
			"ruletree_set_inodestat: add to tree");
		handle->rfh_offs = ruletree_create_inodestat(istat_struct);
		bt_root = ruletree_add_to_bintree_entry(handle->rfh_offs,
			ino_to_key(handle->rfh_ino), handle->rfh_dev,
			handle->rfh_last_visited_node, handle->rfh_last_result);
		if (bt_root)
			ruletree_catalog_set("vperm", "inodestats", bt_root);
		return (bt_root);
	}
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
		catalog_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
		if (!catalog_offs) return(NULL);
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
 * returns location of the new entry and pointer to the new entry in *entry_ptr.
*/
static ruletree_object_offset_t add_entry_to_ruletree_catalog(
	ruletree_object_offset_t	catalog_offs,
	const char			*name,
	ruletree_object_offset_t	value_offs,
	ruletree_catalog_entry_t	**entry_ptr)
{
	ruletree_object_offset_t	entry_location;
	ruletree_catalog_entry_t	*ep = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"%s: add %s=%d to catalog @%d%s",
		__func__, name, (int)value_offs, (int)catalog_offs,
		catalog_offs ? "" : " (root catalog)");

	if (!ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: no HDR, can't add anything.", __func__);
		return(0);
	}

	if (ruletree_ctx.rtree_ruletree_fd < 0) {
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: no FD, can't add anything.", __func__);
		return(0);
	}

	SB_LOG(SB_LOGLEVEL_NOISE,
		"%s: catalog @%d, name=%s",
		__func__, (int)catalog_offs, name);
	entry_location = ruletree_create_catalog_entry(name, value_offs);
	if (!entry_location) {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s: failed to create catalog entry for '%s'",
			__func__, name);
	} else {
		link_entry_to_ruletree_catalog(catalog_offs, entry_location);
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s: new entry '%s' @%d",
			__func__, name, (int)entry_location);
		ep = offset_to_ruletree_object_ptr(entry_location,
			SB2_RULETREE_OBJECT_TYPE_CATALOG);
	}
	*entry_ptr = ep;
	return(entry_location);
}

/* return value = offset of the entry, and *entry_ptr
 * points to the entry (0 and NULL if entry was not found)
*/
static ruletree_object_offset_t ruletree_find_catalog_entry(
	ruletree_object_offset_t	catalog_offs,
	const char			*name,
	ruletree_catalog_entry_t	**entry_ptr)
{
	ruletree_catalog_entry_t	*ep;
	ruletree_object_offset_t entry_location = 0;
	const char	*entry_name;
	size_t		name_len;

	*entry_ptr = NULL;
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
			*entry_ptr = ep;
			return(entry_location);
		}
		entry_location = ep->rtree_cat_next_entry_offs;
	} while (entry_location != 0);

	SB_LOG(SB_LOGLEVEL_NOISE3,
		"'%s' not found", name);
	return(0);
}

ruletree_object_offset_t	ruletree_catalog_find_value_from_catalog(
	ruletree_object_offset_t	first_catalog_entry_offs,
	const char			*name)
{
	ruletree_object_offset_t	object_offs = 0;
	ruletree_catalog_entry_t	*object_cat_entry = NULL;

	object_offs = ruletree_find_catalog_entry(
		first_catalog_entry_offs, name, &object_cat_entry);
	if (!object_offs || !object_cat_entry) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"%s: Failed: %s not found",
			__func__, name);
		return(0);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"%s: Found value=@%d",
		__func__, (int)object_cat_entry->rtree_cat_value_offs);
	return(object_cat_entry->rtree_cat_value_offs);
}

static ruletree_catalog_entry_t *ruletree_catalog_add_or_find_object(
	ruletree_object_offset_t	first_catalog_entry_offs,
	const char			*object_name,
	ruletree_catalog_entry_t	*parent_cat_entry)
{
	ruletree_object_offset_t	object_offs = 0;
	ruletree_catalog_entry_t	*object_cat_entry = NULL;

	SB_LOG(SB_LOGLEVEL_NOISE2, "%s: '%s'", __func__, object_name);
	if (first_catalog_entry_offs) {
		/* the catalog already has something */
		object_offs = ruletree_find_catalog_entry(
			first_catalog_entry_offs, object_name, &object_cat_entry);
		if (!object_offs) {
			object_offs = add_entry_to_ruletree_catalog(
				first_catalog_entry_offs, object_name, 0, &object_cat_entry);
		}
		return(object_cat_entry);
	} 
	/* else there is nothing in the catalog, add object */
	if (parent_cat_entry) {
		/* a subcatalog */
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"%s: add first entry '%s'", __func__, object_name);
		object_offs = ruletree_create_catalog_entry(object_name, 0);
		parent_cat_entry->rtree_cat_value_offs = object_offs;
		object_cat_entry = offset_to_ruletree_object_ptr(object_offs,
			SB2_RULETREE_OBJECT_TYPE_CATALOG);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"%s: first entry to root catalog, '%s'",
			__func__, object_name);
		/* root catalog does not exist, try to create it first. */
		object_offs = add_entry_to_ruletree_catalog(
			0/*root catalog*/, object_name, 0/*value*/, &object_cat_entry);
	}
	return(object_cat_entry);
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
	ruletree_object_offset_t	subcatalog_start_offs = 0;

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
	subcatalog_start_offs = ruletree_catalog_find_value_from_catalog(
		0/*root catalog*/, catalog_name);
	if (!subcatalog_start_offs) return(0);

	return (ruletree_catalog_find_value_from_catalog(subcatalog_start_offs,
		object_name));
}

ruletree_object_offset_t ruletree_catalog_vget(const char *namev[])
{
	int				i;
	ruletree_object_offset_t	catalog_start_offs = 0;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) ruletree_to_memory();

	if (!ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"%s: Failed, no rule tree", __func__);
		return (0);
	}

	catalog_start_offs = 0; /* start from the root catalog */
	for (i = 0; namev[i]; i++) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"%s [%d] '%s'", __func__, i, namev[i]);

		catalog_start_offs = ruletree_catalog_find_value_from_catalog(
			catalog_start_offs, namev[i]);
		if (!catalog_start_offs) return(0);
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"%s returns %d", __func__, (int)catalog_start_offs);
	return (catalog_start_offs);
}

const char *ruletree_catalog_get_string(
	const char *catalog_name,
	const char *object_name)
{
	ruletree_object_offset_t offs;

	if (!ruletree_ctx.rtree_ruletree_path) ruletree_to_memory();

	offs = ruletree_catalog_get(catalog_name, object_name);
	if (offs) {
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
	if (offs) {
		uint32_t *uip = ruletree_get_pointer_to_uint32(offs);
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_get_uint32_ptr(%s,%s): 0x%p",
			catalog_name, object_name, uip);
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
			"ruletree_catalog_get_boolean_ptr(%s,%s): 0x%p",
			catalog_name, object_name, uip);
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
	ruletree_object_offset_t	subcatalog_start_offs = 0;
	ruletree_catalog_entry_t	*catalog_entry_ptr_in_root_catalog = NULL;
	ruletree_catalog_entry_t	*object_cat_entry = NULL;

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
	catalog_entry_ptr_in_root_catalog = ruletree_catalog_add_or_find_object(
		root_catalog_offs, catalog_name, NULL/*no parent*/);
	if (!catalog_entry_ptr_in_root_catalog) return(0);

	subcatalog_start_offs = catalog_entry_ptr_in_root_catalog->rtree_cat_value_offs;
	object_cat_entry = ruletree_catalog_add_or_find_object(
		subcatalog_start_offs, object_name,
		catalog_entry_ptr_in_root_catalog);
	if (object_cat_entry) {
		object_cat_entry->rtree_cat_value_offs = value_offset;
		return(1);
	}
	return (0);
}

int ruletree_catalog_vset(
	const char	*namev[],
	ruletree_object_offset_t value_offset)
{
	ruletree_object_offset_t	catalog_start_offs = 0;
	ruletree_catalog_entry_t	*catptr = NULL;
	int				i;

	if (!ruletree_ctx.rtree_ruletree_hdr_p) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"ruletree_catalog_set: Failed, no rule tree");
		return (0);
	}

	catalog_start_offs = ruletree_ctx.rtree_ruletree_hdr_p->rtree_hdr_root_catalog;
	for (i = 0; namev[i]; i++) {
		SB_LOG(SB_LOGLEVEL_NOISE2, "%s [%d] %s", __func__, i, namev[i]);

		catptr = ruletree_catalog_add_or_find_object(
			catalog_start_offs, namev[i], catptr);
		if (!catptr) return(0);

		catalog_start_offs = catptr->rtree_cat_value_offs;
	}
	if (catptr) {
		SB_LOG(SB_LOGLEVEL_NOISE2, "%s Found, set to %d", __func__, (int)value_offset);
		catptr->rtree_cat_value_offs = value_offset;
		return(1);
	}
	return (0);
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

