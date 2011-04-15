/*
 * Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 * Portion Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Interfaces to networking code Lua functions.
*/

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "mapping.h"
#include "sb2.h"
#include "sb2_network.h"
#include "libsb2.h"
#include "exported.h"

#if 0
#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif
#endif

/* ========== Interfaces to Lua functions: ========== */

int call_lua_function_sbox_map_network_addr(
	const char *binary_name,
	const char *realfnname,
	const char *protocol,
	const char *addr_type,
	const char *orig_dst_addr,
	int orig_port,
	char *result_addr_buf,
	int result_addr_buf_len,
	int *result_port)
{
	struct lua_instance	*luaif = NULL;
	char *result_code = NULL;
	int return_value = 0;
	char *log_level = NULL;

	luaif = get_lua();
        if (!luaif) return(EBADF);	/* overload EBADF; if get_lua() fails,
					 * things are in a bad state anyway */

	SB_LOG(SB_LOGLEVEL_NOISE, "calling sbox_map_network_addr for %s:%d",
		orig_dst_addr, orig_port);
	SB_LOG(SB_LOGLEVEL_NOISE, "binary_name = '%s'", binary_name);

	lua_getfield(luaif->lua, LUA_GLOBALSINDEX, "sbox_map_network_addr");
	/* add parameters */
	lua_pushstring(luaif->lua, realfnname);
	lua_pushstring(luaif->lua, protocol);
	lua_pushstring(luaif->lua, addr_type);
	lua_pushstring(luaif->lua, orig_dst_addr);
	lua_pushnumber(luaif->lua, orig_port);
	lua_pushstring(luaif->lua, binary_name);

	if(SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_sbox_map_network_addr -> call",
			luaif->lua);
	}
	/* 6 arguments, returns result_code,address,port,log_level,log_msg */
	lua_call(luaif->lua, 6, 5);

	result_code = (char *)lua_tostring(luaif->lua, -5); /* result_code */
	if ((*result_code == '\0') || (!strcmp(result_code, "OK"))) {
		/* empty string or "OK" == success */
		char *result_addr = (char *)lua_tostring(luaif->lua, -4);
		snprintf(result_addr_buf, result_addr_buf_len, "%s", result_addr);
		*result_port = lua_tointeger(luaif->lua, -3);
		SB_LOG(SB_LOGLEVEL_NOISE, "sbox_map_network_addr => OK, addr=%s, port=%d",
			result_addr_buf, *result_port);
	} else {
		/* following errno values can be set from sbox_map_network_addr().
		 * Especially EACCES and EPERM are useful with connect(), they can
		 * signify that that a local firewall rule caused
		 * the failure; pretty neat for our purposes. */
		if (!strcmp(result_code, "ENETUNREACH")) return_value = ENETUNREACH;
		else if (!strcmp(result_code, "EACCES")) return_value = EACCES;
		else if (!strcmp(result_code, "EPERM")) return_value = EPERM;
		else if (!strcmp(result_code, "EFAULT")) return_value = EFAULT;
		else if (!strcmp(result_code, "EADDRNOTAVAIL")) return_value = EADDRNOTAVAIL;
		else if (!strcmp(result_code, "EADDRINUSE")) return_value = EADDRINUSE;
		else return_value = EACCES; /* the default */
		SB_LOG(SB_LOGLEVEL_NOISE, "sbox_map_network_addr => ERROR, code=%s => #%d",
			result_code, return_value);
	}
	log_level = (char *)lua_tostring(luaif->lua, -2);
	if (log_level) {
		char *log_msg = (char *)lua_tostring(luaif->lua, -1);
		if (log_msg) {
			int lvl = SB_LOGLEVEL_ERROR; /* default */
			if(!strcmp(log_level, "debug"))
				lvl = SB_LOGLEVEL_DEBUG;
			else if(!strcmp(log_level, "info"))
				lvl = SB_LOGLEVEL_INFO;
			else if(!strcmp(log_level, "warning"))
				lvl = SB_LOGLEVEL_WARNING;
			else if(!strcmp(log_level, "network"))
				lvl = SB_LOGLEVEL_NETWORK;
			else if(!strcmp(log_level, "notice"))
				lvl = SB_LOGLEVEL_NOTICE;
			else if(!strcmp(log_level, "error"))
				lvl = SB_LOGLEVEL_ERROR;
			else if(!strcmp(log_level, "noise"))
				lvl = SB_LOGLEVEL_NOISE;
			else if(!strcmp(log_level, "noise2"))
				lvl = SB_LOGLEVEL_NOISE2;
			else if(!strcmp(log_level, "noise3"))
				lvl = SB_LOGLEVEL_NOISE3;
			SB_LOG(lvl, "%s: %s", realfnname, log_msg);
		}
	}

	lua_pop(luaif->lua, 5); /* drop return values from the stack */
	return(return_value);
}

static int compare_ipv4_addrs(const char *address, const char *address_pattern)
{
	struct in_addr addr;
	uint32_t addr32;

	/* first test with strcmp.. the easy & fast way */
	if (!strcmp(address, address_pattern)) return(1);

	/* "address" must be in numeric format (e.g. "1.2.3.4") */
	if (inet_pton(AF_INET, address, (void*)&addr) != 1) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "inet_pton(addr:%s) FAILED",
			address);
		/* can't compare it anyway. */
		return(0);
	}
	addr32 = htonl(addr.s_addr);

	/* If the pattern is in numeric format:
	 * - "1.2.3.4" requires an exact match
	 * - "1.2.3.0/24" requires that the network part matches.
	*/
	if (isdigit(*address_pattern)) {
		char *s_mask = NULL;

		s_mask = strchr(address_pattern, '/');
		if (s_mask) {
			int mask_bits = atoi(s_mask+1);
			if ((mask_bits > 0) && (mask_bits <= 32)) {
				struct in_addr mask;
				uint32_t bitmask, addr32_masked, mask32;
				char *mask_copy = strdup(address_pattern);
				char *cp = strchr(mask_copy, '/');

				*cp = '\0'; /* cut mask_copy */
				switch(inet_pton(AF_INET, mask_copy,
					(void*)&mask)) {
				case 1: /* success */
					mask32 = htonl(mask.s_addr);
					bitmask = 0xFFFFFFFF << (32-mask_bits);
					addr32_masked = addr32 & bitmask;
					SB_LOG(SB_LOGLEVEL_NOISE,
						"cmp ipv4 addr 0x%X 0x%X %d;"
						" %0xX (0x%X)",
						addr32, mask32, mask_bits,
						addr32_masked, bitmask);
					if (addr32_masked == mask32) return(1);
					break;
				default:
					SB_LOG(SB_LOGLEVEL_NOISE,
						"inet_pton(mask:%s) FAILED",
						mask_copy);
				}
			} else {
				SB_LOG(SB_LOGLEVEL_ERROR,
					"incorrect number of bits in IPv4 subnet mask (%d)",
					mask_bits);
			}
		} /* else no '/' in address_pattern */
	} else {
		/* first char of the pattern was not a digit,
		 * check special cases: */

		/* INADDR_ANY does *not* mean that any address
		 * will match: It matches only if the address
		 * *is* INADDR_ANY, exactly. */
		if (!strcmp(address_pattern, "INADDR_ANY") &&
		    (addr32 == INADDR_ANY) ) {
			return(1);
		}
	}
	return(0); /* no match */
}

/* "sb.test_net_addr_match(addr_type, address, address_pattern)":
 * This is used from find_net_rule(); implementing this in C improves preformance.
 * Returns result (true if matched, false if it didn't)
 * and status string ("exact" or "net" if matched, or error msg. if it didn't match)
*/
int lua_sb_test_net_addr_match(lua_State *l)
{
	int	n = lua_gettop(l);
	int	result = 0;	/* false */
	const char	*match_type = "No match";

	if (n == 3) {
		const char	*addr_type = lua_tostring(l, 1);
		const char	*address = lua_tostring(l, 2);
		const char	*address_pattern = lua_tostring(l, 3);

		SB_LOG(SB_LOGLEVEL_NOISE2,
			"lua_sb_test_net_addr_match '%s','%s','%s'",
			addr_type, address, address_pattern);

		if (addr_type && !strncmp(addr_type, "ipv4", 4)) {
			result = compare_ipv4_addrs(address, address_pattern);
			match_type = "ipv4 test";
		} else {
			match_type = "Unsupported address type";
		}
	}
	SB_LOG(SB_LOGLEVEL_NOISE2,
		"lua_sb_test_net_addr_match => %d (%s)", result, match_type);
	lua_pushboolean(l, result);
	lua_pushstring(l, match_type);
	return 2;
}

/* ----- EXPORTED from interface.master: ----- */
int sb2show__map_network_addr__(
	const char *binary_name, const char *fn_name,
	const char *protocol, const char *addr_type,
	const char *dst_addr, int port,
	char **addr_bufp, int *new_portp)
{
	int res;
	char result_buf[200];

	if (!addr_type || !protocol || !dst_addr || !addr_bufp || !new_portp) {
		*addr_bufp = strdup("Parameter error");
		return(-1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s '%s'", __func__, dst_addr);

	result_buf[0] = '\0';
	res = call_lua_function_sbox_map_network_addr(
		binary_name, fn_name, protocol,
		addr_type, dst_addr, port,
		result_buf, sizeof(result_buf),
		new_portp);
	*addr_bufp = strdup(result_buf);
	return(res);
}

