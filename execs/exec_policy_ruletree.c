/*
 * Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "exported.h"

#include <sys/mman.h>

#include "sb2_execs.h"

/* Functions for accessing exec policy settings in the rule tree db.
 *
 * Currenly exec policies are stored using Catalogs; this
 * is simple but not very efficient. This interface makes it
 * easy to change that later to use e.g. a binary structure,
 * similar as what is already used for mapping rules etc.
*/

exec_policy_handle_t     find_exec_policy_handle(const char *policyname)
{
	exec_policy_handle_t	eph;
	const char *v[4];
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: find %s", __func__, policyname);
	v[0] = "exec_policy";
	v[1] = sbox_session_mode ? sbox_session_mode : ruletree_catalog_get_string("MODES", "#default");
	v[2] = policyname;
	v[3] = NULL;
	
	eph.exec_policy_offset = ruletree_catalog_vget(v);
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: Handle for (%s,%s) = %u", __func__,
		v[1], v[2], eph.exec_policy_offset);
	return(eph);
}

const char *exec_policy_get_string(exec_policy_handle_t eph,
        const char *s_name, size_t fldoffs)
{
	if (eph.exec_policy_offset) {
		ruletree_object_offset_t offs;
		const char *str;

		offs = ruletree_catalog_find_value_from_catalog(
			eph.exec_policy_offset, s_name);
		str = offset_to_ruletree_string_ptr(offs, NULL);
                SB_LOG(SB_LOGLEVEL_NOISE,
                        "%s: %s='%s'", __func__, s_name, str, fldoffs);
                return(str);
	}
	return(NULL);
}

int exec_policy_get_boolean(exec_policy_handle_t eph,
        const char *b_name, size_t fldoffs)
{
	if (eph.exec_policy_offset) {
		ruletree_object_offset_t offs;
		uint32_t *uip;

		offs = ruletree_catalog_find_value_from_catalog(
			eph.exec_policy_offset, b_name);
		uip = ruletree_get_pointer_to_boolean(offs);
                SB_LOG(SB_LOGLEVEL_NOISE,
                        "%s: @%u = *%p = %d",
			__func__, offs, uip, (uip ? *uip : 999), fldoffs);
                return(uip ? * uip : 0);
	}
	return(0);
}

ruletree_object_offset_t exec_policy_get_rules(exec_policy_handle_t eph,
        const char *r_name, size_t fldoffs)
{
	if (eph.exec_policy_offset) {
		ruletree_object_offset_t offs;

		offs = ruletree_catalog_find_value_from_catalog(
			eph.exec_policy_offset, r_name);
                SB_LOG(SB_LOGLEVEL_NOISE,
                        "%s: @%u", __func__, offs, fldoffs);
                return(offs);
	}
	return(0);
}

