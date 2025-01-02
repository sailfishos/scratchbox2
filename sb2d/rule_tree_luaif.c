/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */
#include <config.h>

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
#undef NDEBUG
#include <assert.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <mapping.h>
#include <sb2.h>
#include <rule_tree.h>

#include "rule_tree_lua.h"

/* ensure that the rule tree has been mapped. */
static int lua_sb_attach_ruletree(lua_State *l)
{
	int attach_result = -1;

	if (sbox_session_dir) {
		char *rule_tree_path = NULL;

		/* map the rule tree to memory: */
		assert(asprintf(&rule_tree_path, "%s/RuleTree.bin", sbox_session_dir) > 0);
		attach_result = attach_ruletree(rule_tree_path, 1/*keep open*/);
		SB_LOG(SB_LOGLEVEL_DEBUG, "attach(%s) = %d", rule_tree_path, attach_result);
		free(rule_tree_path);
	}
	lua_pushnumber(l, attach_result);
	SB_LOG(SB_LOGLEVEL_DEBUG, "lua_sb_attach_ruletree => %d",
		attach_result);
	return(1);
}

/* "sb.add_rule_to_ruletree(...)
*/
static int lua_sb_add_rule_to_ruletree(lua_State *l)
{
	int	n = lua_gettop(l);
	uint32_t rule_location = 0;

	if (n == 12) {
		const char	*rule_name = lua_tostring(l, 1);
		int		selector_type = lua_tointeger(l, 2);
		const char	*selector = lua_tostring(l, 3);
		int		action_type = lua_tointeger(l, 4);
		const char	*action_str = lua_tostring(l, 5);
		int		condition_type = lua_tointeger(l, 6);
		const char	*condition_str = lua_tostring(l, 7);
		ruletree_object_offset_t rule_list_link = lua_tointeger(l, 8);
		int		flags = lua_tointeger(l, 9);
		const char	*binary_name = lua_tostring(l, 10);
		int		func_class = lua_tointeger(l, 11);
		const char	*exec_policy_name = lua_tostring(l, 12);

		rule_location = add_rule_to_ruletree(rule_name,
			selector_type, selector,
			action_type, action_str,
			condition_type, condition_str,
			rule_list_link,
			flags, binary_name, func_class, exec_policy_name);

		SB_LOG(SB_LOGLEVEL_NOISE,
			"lua_sb_add_rule_to_ruletree '%s' => %d",
			rule_name, rule_location);
	}
	lua_pushnumber(l, rule_location);
	return 1;
}

/* ruletree.add_exec_preprocessing_rule_to_ruletree(...)
*/
static int lua_sb_add_exec_preprocessing_rule_to_ruletree(lua_State *l)
{
	int	n = lua_gettop(l);
	uint32_t rule_location = 0;

	if (n == 8) {
		const char	*binary_name = lua_tostring(l, 1);
		ruletree_object_offset_t  path_prefixes_table_offs = lua_tointeger(l, 2);
		ruletree_object_offset_t  add_head_table_offs = lua_tointeger(l, 3);
		ruletree_object_offset_t  add_options_table_offs = lua_tointeger(l, 4);
		ruletree_object_offset_t  add_tail_table_offs = lua_tointeger(l, 5);
		ruletree_object_offset_t  remove_table_offs = lua_tointeger(l, 6);
		const char	*new_filename = lua_tostring(l, 7);
		int		disable_mapping = lua_tointeger(l, 8);

		rule_location = add_exec_preprocessing_rule_to_ruletree(
			binary_name, path_prefixes_table_offs,
			add_head_table_offs, add_options_table_offs,
			add_tail_table_offs, remove_table_offs,
			new_filename, disable_mapping);

		SB_LOG(SB_LOGLEVEL_NOISE,
			"lua_sb_add_exec_preprocessing_rule_to_ruletree '%s' => %d",
			binary_name, rule_location);
	}
	lua_pushnumber(l, rule_location);
	return 1;
}

static int lua_sb_ruletree_objectlist_create_list(lua_State *l)
{
	int				n = lua_gettop(l);
	ruletree_object_offset_t	list_offs = 0;

	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_objectlist_create_list n=%d", n);
	if (n == 1) {
		uint32_t	list_size = lua_tointeger(l, 1);
		list_offs = ruletree_objectlist_create_list(list_size);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_objectlist_create_list => %d", list_offs);
	lua_pushnumber(l, list_offs);
	return 1;
}

static int lua_sb_ruletree_objectlist_set_item(lua_State *l)
{
	int	n = lua_gettop(l);
	int	res = 0;

	if (n == 3) {
		ruletree_object_offset_t	list_offs = lua_tointeger(l, 1);
		uint32_t			n = lua_tointeger(l, 2);
		ruletree_object_offset_t	value = lua_tointeger(l, 3);
		res = ruletree_objectlist_set_item(list_offs, n, value);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_objectlist_set_item => %d", res);
	return 0;
}

static int lua_sb_ruletree_objectlist_get_item(lua_State *l)
{
	int				n = lua_gettop(l);
	ruletree_object_offset_t	value = 0;

	if (n == 2) {
		ruletree_object_offset_t	list_offs = lua_tointeger(l, 1);
		uint32_t			n = lua_tointeger(l, 2);
		value = ruletree_objectlist_get_item(list_offs, n);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_objectlist_get_item => %d", value);
	lua_pushnumber(l, value);
	return 1;
}

static int lua_sb_ruletree_objectlist_get_list_size(lua_State *l)
{
	int		n = lua_gettop(l);
	uint32_t	size = 0;

	if (n == 1) {
		ruletree_object_offset_t	list_offs = lua_tointeger(l, 1);
		size = ruletree_objectlist_get_list_size(list_offs);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_objectlist_get_list_size => %d", size);
	lua_pushnumber(l, size);
	return 1;
}

static int lua_sb_ruletree_catalog_get(lua_State *l)
{
	int				n = lua_gettop(l);
	ruletree_object_offset_t	value = 0;

	if (n == 2) {
		const char	*catalog_name = lua_tostring(l, 1);
		const char	*object_name = lua_tostring(l, 2);
		value = ruletree_catalog_get(catalog_name, object_name);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_catalog_get => %d", (int)value);
	lua_pushnumber(l, value);
	return 1;
}

static int lua_sb_ruletree_catalog_get_uint32(lua_State *l)
{
	int		n = lua_gettop(l);
	uint32_t	res = 0;
	ruletree_object_offset_t	offs = 0;

	if (n == 2) {
		const char	*catalog_name = lua_tostring(l, 1);
		const char	*object_name = lua_tostring(l, 2);
		uint32_t			*uip = 0;

		offs = ruletree_catalog_get(catalog_name, object_name);
		uip = ruletree_get_pointer_to_uint32(offs);
		if (uip) res = *uip;
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_catalog_get_uint32 @%u => %u", offs, res);
	lua_pushnumber(l, res);
	return 1;
}

static int lua_sb_ruletree_catalog_get_boolean(lua_State *l)
{
	int		n = lua_gettop(l);
	uint32_t	res = 0;
	ruletree_object_offset_t	offs = 0;

	if (n == 2) {
		const char	*catalog_name = lua_tostring(l, 1);
		const char	*object_name = lua_tostring(l, 2);
		uint32_t			*uip = 0;

		offs = ruletree_catalog_get(catalog_name, object_name);
		uip = ruletree_get_pointer_to_boolean(offs);
		if (uip) res = *uip;
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_catalog_get_boolean @%u => %u", offs, res);
	lua_pushboolean(l, res);
	return 1;
}

static int lua_sb_ruletree_catalog_get_string(lua_State *l)
{
	int		n = lua_gettop(l);
	ruletree_object_offset_t	offs = 0;
	const char	*str = NULL;

	if (n == 2) {
		const char	*catalog_name = lua_tostring(l, 1);
		const char	*object_name = lua_tostring(l, 2);

		str = ruletree_catalog_get_string(catalog_name, object_name);
	}
	SB_LOG(SB_LOGLEVEL_NOISE,
		"lua_sb_ruletree_catalog_get_string @%u => %s", offs, str);
	lua_pushstring(l, str);
	return 1;
}


static int lua_sb_ruletree_catalog_set(lua_State *l)
{
	int	n = lua_gettop(l);
	int	status = 0;

	if (n == 3) {
		const char	*catalog_name = lua_tostring(l, 1);
		const char	*object_name = lua_tostring(l, 2);
		ruletree_object_offset_t	value_offset = lua_tointeger(l, 3);
		status = ruletree_catalog_set(catalog_name, object_name, value_offset);
		SB_LOG(SB_LOGLEVEL_NOISE,
			"lua_sb_ruletree_catalog_set(%s,%s,%d) => %d",
			catalog_name, object_name, value_offset, status);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"lua_sb_ruletree_catalog_set => %d", status);
	}
	lua_pushnumber(l, status);
	return 1;
}

static int lua_sb_ruletree_catalog_vget(lua_State *l)
{
	int				n = lua_gettop(l);
	ruletree_object_offset_t	value = 0;

	if (n >= 1) {
		char	**namev;
		int	i;

		namev = calloc(n+1, sizeof(char*));
		for (i = 0; i < n; i++) {
			namev[i] = strdup(lua_tostring(l, i+1));
		}
		value = ruletree_catalog_vget((const char**)namev);
		for (i = 0; i < n; i++) {
			if(namev[i]) free(namev[i]);
			namev[i] = NULL;
		}
		free(namev);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "%s => %d", __func__, (int)value);
	lua_pushnumber(l, value);
	return 1;
}

static int lua_sb_ruletree_catalog_vset(lua_State *l)
{
	int	n = lua_gettop(l);
	int	status = 0;

	if (n >= 2) {
		char	**namev;
		int	i;
		ruletree_object_offset_t	value_offset;

		namev = calloc(n, sizeof(char*));
		for (i = 0; i < n-1; i++) {
			namev[i] = strdup(lua_tostring(l, i+1));
		}
		value_offset = lua_tointeger(l, n);
		status = ruletree_catalog_vset((const char**)namev, value_offset);
		for (i = 0; i < n-1; i++) {
			if(namev[i]) free(namev[i]);
			namev[i] = NULL;
		}
		free(namev);
	}
	SB_LOG(SB_LOGLEVEL_NOISE, "%s => %d", __func__, status);
	lua_pushnumber(l, status);
	return 1;
}

static int lua_sb_ruletree_new_string(lua_State *l)
{
	int	n = lua_gettop(l);
	ruletree_object_offset_t	str_offs = 0;

	if (n == 1) {
		const char	*str = lua_tostring(l, 1);
		if (str) {
			str_offs = append_string_to_ruletree_file(str);
			SB_LOG(SB_LOGLEVEL_NOISE,
				"lua_sb_ruletree_new_string(%s) => %d", str, str_offs);
		} else {
			SB_LOG(SB_LOGLEVEL_NOISE,
				"lua_sb_ruletree_new_string(NULL) => %d", str_offs);
		}
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"lua_sb_ruletree_new_string => %d", str_offs);
	}
	lua_pushnumber(l, str_offs);
	return 1;
}

static int lua_sb_ruletree_new_uint32(lua_State *l)
{
	int	n = lua_gettop(l);
	ruletree_object_offset_t	ui32_offs = 0;

	if (n == 1) {
		uint32_t	ui = lua_tointeger(l, 1);
		ui32_offs = append_uint32_to_ruletree_file(ui);
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s(%u) => %d", __func__, ui, ui32_offs);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s => %d", __func__, ui32_offs);
	}
	lua_pushnumber(l, ui32_offs);
	return 1;
}

static int lua_sb_ruletree_new_boolean(lua_State *l)
{
	int	n = lua_gettop(l);
	ruletree_object_offset_t	ui32_offs = 0;

	if (n == 1) {
		uint32_t	ui = lua_toboolean(l, 1);
		ui32_offs = append_boolean_to_ruletree_file(ui);
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s(%u) => %d", __func__, ui, ui32_offs);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s => %d", __func__, ui32_offs);
	}
	lua_pushnumber(l, ui32_offs);
	return 1;
}

static int lua_sb_add_exec_policy_selection_rule_to_ruletree(lua_State *l)
{
	int	n = lua_gettop(l);
	uint32_t rule_location = 0;

	if (n == 4) {
		uint32_t	eps_ruletype = lua_tointeger(l, 1);
		const char	*eps_selector = lua_tostring(l, 2);
		const char	*exec_policy_name = lua_tostring(l, 3);
		uint32_t	eps_flags = lua_tointeger(l, 4);

		rule_location = add_exec_policy_selection_rule_to_ruletree(
			eps_ruletype, eps_selector, exec_policy_name, eps_flags);

		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s %d '%s' '%s' => %d",
			__func__, eps_ruletype, eps_selector, exec_policy_name, rule_location);
	}
	lua_pushnumber(l, rule_location);
	return 1;
}

static int lua_sb_add_net_rule_to_ruletree(lua_State *l)
{
	int	n = lua_gettop(l);
	uint32_t rule_location = 0;

	/* params: 
	 *  1. ruletype (int),
	 *  2. func_name (offset of a string) 
	 *  3. binary_name (offset of a string)
	 *  4. address (offset of a string)
	 *  5. port (int)
	 *  6. new_address (offset of a string)
	 *  7. new_port (int)
	 *  8. log_level (string; converted to a number here)
	 *  9. log_msg (offset of a string)
	 *  10. errno (offset of a string)
	 *  11. subrules (offset to a list)
	*/
	if (n == 11) {
		ruletree_net_rule_t	rule;
		const char *errno_str;
		
		SB_LOG(SB_LOGLEVEL_NOISE, "%s:", __func__);

		rule.rtree_net_ruletype = lua_tointeger(l, 1);
		rule.rtree_net_func_name = lua_tointeger(l, 2);
		rule.rtree_net_binary_name = lua_tointeger(l, 3);
		rule.rtree_net_address = lua_tointeger(l, 4);
		rule.rtree_net_port = lua_tointeger(l, 5);
		rule.rtree_net_new_address = lua_tointeger(l, 6);
		rule.rtree_net_new_port = lua_tointeger(l, 7);
		rule.rtree_net_log_level = sblog_level_name_to_number(lua_tostring(l, 8));
		rule.rtree_net_log_msg = lua_tointeger(l, 9);
		errno_str = lua_tostring(l,10);
		rule.rtree_net_rules = lua_tointeger(l, 11);

		if (errno_str) {
			int e;

			/* following errno values can be set in networking rules.
			 * Especially EACCES and EPERM are useful with connect(), they can
			 * signify that that a local firewall rule caused
			 * the failure; pretty neat for our purposes. */
			if (!strcmp(errno_str, "ENETUNREACH")) e = ENETUNREACH;
			else if (!strcmp(errno_str, "EACCES")) e = EACCES;
			else if (!strcmp(errno_str, "EPERM")) e = EPERM;
			else if (!strcmp(errno_str, "EFAULT")) e = EFAULT;
			else if (!strcmp(errno_str, "EADDRNOTAVAIL")) e = EADDRNOTAVAIL;
			else if (!strcmp(errno_str, "EADDRINUSE")) e = EADDRINUSE;
			else {
				SB_LOG(SB_LOGLEVEL_NOISE, "errno: use default (EACCESS)");
				e = EACCES; /* the default */
			}
			SB_LOG(SB_LOGLEVEL_NOISE, "errno = %s => #%d", errno_str, e);
			rule.rtree_net_errno = e;
		} else {
			rule.rtree_net_errno = 0;
		}

		rule_location = add_net_rule_to_ruletree(&rule);

		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s => %d", __func__, rule_location);
	}
	lua_pushnumber(l, rule_location);
	return 1;
}

/* mappings from c to lua */
static const luaL_Reg reg[] =
{
	{"objectlist_create",		lua_sb_ruletree_objectlist_create_list},
	{"objectlist_set",		lua_sb_ruletree_objectlist_set_item},
	{"objectlist_get",		lua_sb_ruletree_objectlist_get_item},
	{"objectlist_size",		lua_sb_ruletree_objectlist_get_list_size},

	/* simple interface to catalogs (two levels) */
	{"catalog_get",			lua_sb_ruletree_catalog_get},
	{"catalog_set",			lua_sb_ruletree_catalog_set},
	{"catalog_get_uint32",		lua_sb_ruletree_catalog_get_uint32},
	{"catalog_get_boolean",		lua_sb_ruletree_catalog_get_boolean},
	{"catalog_get_string",		lua_sb_ruletree_catalog_get_string},

	/* alternative interface to catalogs (variable levels) */
	{"catalog_vget",		lua_sb_ruletree_catalog_vget},
	{"catalog_vset",		lua_sb_ruletree_catalog_vset},

	/* 'ruletree.catalog_set("catalogname","itemname",
	 *   ruletree.new_string("str")' can be used from Lua to
	 * store configuration variables to rule tree.
	*/
	{"new_string",			lua_sb_ruletree_new_string},
	{"new_uint32",			lua_sb_ruletree_new_uint32},
	{"new_boolean",			lua_sb_ruletree_new_boolean},

	{"attach_ruletree",		lua_sb_attach_ruletree},

	/* FS rules */
	{"add_rule_to_ruletree",	lua_sb_add_rule_to_ruletree},

	/* exec rules */
	{"add_exec_preprocessing_rule_to_ruletree",	lua_sb_add_exec_preprocessing_rule_to_ruletree},
	{"add_exec_policy_selection_rule_to_ruletree",	lua_sb_add_exec_policy_selection_rule_to_ruletree},

	/* Network rules */
	{"add_net_rule_to_ruletree",	lua_sb_add_net_rule_to_ruletree},

	{NULL,				NULL}
};


int lua_bind_ruletree_functions(lua_State *l)
{
        lua_newtable(l);
	luaL_setfuncs(l, reg, 0);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);
        lua_setglobal(l,  "ruletree");

	return 0;
}

