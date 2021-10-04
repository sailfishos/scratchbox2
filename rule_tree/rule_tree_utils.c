/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <assert.h>

#include "sb2.h"
#include "rule_tree.h"

/* ----- utility functions for managing counters in the "vperm" catalog ----- */

static ruletree_object_offset_t	num_active_inodestats_offs = 0;
static volatile uint32_t	*num_active_inodestats_ptr = NULL;

static void locate_status_variables_in_ruletree(void)
{
	num_active_inodestats_offs = ruletree_catalog_get(
		"vperm", "num_active_inodestats");
	if (num_active_inodestats_offs) {
		num_active_inodestats_ptr = ruletree_get_pointer_to_uint32(
			num_active_inodestats_offs);
	}
}

void inc_vperm_num_active_inodestats(void)
{
	if (!num_active_inodestats_offs) 
		locate_status_variables_in_ruletree();
	if (!num_active_inodestats_ptr) return;

	if (*num_active_inodestats_ptr < (uint32_t)(~0)) {
		(*num_active_inodestats_ptr)++;
	}
}

void dec_vperm_num_active_inodestats(void)
{
	if (!num_active_inodestats_offs) 
		locate_status_variables_in_ruletree();
	if (!num_active_inodestats_ptr) return;

	if (*num_active_inodestats_ptr > 0) {
		(*num_active_inodestats_ptr)--;
	}
}

uint32_t get_vperm_num_active_inodestats(void)
{
	if (!num_active_inodestats_offs) 
		locate_status_variables_in_ruletree();
	if (!num_active_inodestats_ptr) return(0);

	return(*num_active_inodestats_ptr);
}

