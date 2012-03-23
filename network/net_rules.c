/*
 * Copyright (c) 2011,2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
 * Portion Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * networking rule engine.
 *
 * This is a very straightforward conversion from Lua; old
 * Lua code is even preserved in the comments below.
 * The downside is that the code is definitely not optimal
 * (e.g. converts addresses to strings and back, just as
 * the practise was with Lua). But a) the code is faster
 * than the Lua implementation was and b) it is easy to
 * optimize now if it turns out to be a problem. Remember
 * that these routines are usually called for connections, not
 * for every TCP packet (see "NETWORKING MODES" in sb2(1))
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

/* ========== find & execute a network rule for given address: ========== */

static int compare_ipv4_addrs(const char *address, const char *address_pattern)
{
	struct in_addr addr;
	uint32_t addr32;

	/* first test with strcmp.. the easy & fast way */
	if (!strcmp(address, address_pattern)) return(1);

	/* "address" must be in numeric format (e.g. "1.2.3.4") */
	if (inet_pton(AF_INET, address, (void*)&addr) != 1) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "inet_pton(IPv4 addr:%s) FAILED",
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

static struct in6_addr local_in6addr_any = IN6ADDR_ANY_INIT;

static int compare_ipv6_addrs(const char *address, const char *address_pattern)
{
	struct in6_addr addr;

	/* first test with strcmp.. the easy & fast way */
	if (!strcmp(address, address_pattern)) return(1);

	/* "address" must be in numeric format (e.g. "1.2.3.4") */
	if (inet_pton(AF_INET6, address, (void*)&addr) != 1) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "inet_pton(IPv6 addr:%s) FAILED",
			address);
		/* can't compare it anyway. */
		return(0);
	}

	if (!strcmp(address_pattern, "IN6ADDR_ANY")) {
		/* IN6ADDR_ANY does *not* mean that any address
		 * will match: It matches only if the address
		 * *is* IN6ADDR_ANY, exactly. */
		if (!memcmp(&addr, &local_in6addr_any, sizeof(addr))) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"matched IN6ADDR_ANY");
			return(1);
		}
	} else {
		/* "numeric format":
		 * - "3fff:ffff:8:9" requires an exact match
		 * - "3fff:ffff::8:9/32" requires that the network part matches.
		*/
		char *s_mask = NULL;

		s_mask = strchr(address_pattern, '/');
		if (s_mask) {
			int mask_bits = atoi(s_mask+1);

			if ((mask_bits > 0) && (mask_bits <= 128)) {
				struct in6_addr addr_mask;
				struct in6_addr prefix;
				struct in6_addr masked_addr;
				int m = mask_bits;
				int i;
				char *prefix_copy = strdup(address_pattern);
				char *cp = strchr(prefix_copy, '/');

				*cp = '\0'; /* cut prefix_copy */
				memset(&addr_mask, 0, sizeof(addr_mask));
				for (i = 0; m >= 8; i++, m -= 8) {
					addr_mask.s6_addr[i] = 0xFF;
				}
				if (m > 0) {
					addr_mask.s6_addr[i] = ((0xFF << (8 - m)) & 0xFF);
				}
				if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE)) {
					char printable_addr_mask[200];

					if (!inet_ntop(AF_INET6, &addr_mask,
						printable_addr_mask, sizeof(printable_addr_mask))) {
						strcpy(printable_addr_mask, "<addr.conv.error>");
					}
					SB_LOG(SB_LOGLEVEL_NOISE,
						"ipv6 addr mask = %s", printable_addr_mask);
				}
				switch(inet_pton(AF_INET6, prefix_copy,
					(void*)&prefix)) {
				case 1: /* success */
					for (i = 0; i < 16; i++) {
						masked_addr.s6_addr[i] =
							addr_mask.s6_addr[i] & addr.s6_addr[i];
					}
					i = memcmp(&masked_addr, &prefix, sizeof(masked_addr));
					SB_LOG(SB_LOGLEVEL_NOISE,
						"cmp ipv6 addr w. prefix, result = %d", i);
					if (!i) return(1);
					break;
				default:
					SB_LOG(SB_LOGLEVEL_NOISE,
						"inet_pton(IPv6 prefix:%s) FAILED",
						prefix_copy);
				}
			} else {
				SB_LOG(SB_LOGLEVEL_ERROR,
					"incorrect number of bits in IPv6 prefix (%d)",
					mask_bits);
			}
		} /* else no '/' in address_pattern */
	}
	return(0); /* no match */
}

/* test_net_addr_match:
 * Returns result (true if matched, false if it didn't)
*/
static int test_net_addr_match(
	const char *addr_type,
	const char *address,
	const char *address_pattern)
{
	int	result = 0;	/* false: no match */
	const char	*match_type = "No match";

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"test_net_addr_match '%s','%s','%s'",
		addr_type, address, address_pattern);

	if (addr_type && !strncmp(addr_type, "ipv4", 4)) {
		result = compare_ipv4_addrs(address, address_pattern);
		match_type = "ipv4 test";
	} else if (addr_type && !strncmp(addr_type, "ipv6", 4)) {
		result = compare_ipv6_addrs(address, address_pattern);
		match_type = "ipv6 test";
	} else {
		match_type = "Unsupported address type";
	}

	SB_LOG(SB_LOGLEVEL_NOISE2,
		"%s => %d (%s)", __func__, result, match_type);
	return(result);
}

/* Lua:
 *	find_net_rule(netruletable, realfnname, addr_type,
 *	        orig_dst_addr, orig_port, binary_name)
*/
static ruletree_net_rule_t *find_net_rule(
	ruletree_object_offset_t	net_rule_list_offs,
	const char *realfnname,
	const char *addr_type,
	const char *orig_dst_addr,
	unsigned int orig_port,
	const char *binary_name)
{
	unsigned int	i;
	uint32_t	rule_list_size;

	rule_list_size = ruletree_objectlist_get_list_size(net_rule_list_offs);

	SB_LOG(SB_LOGLEVEL_NOISE,
		"%s: %s %s %d, rules @%d",
		__func__, addr_type, orig_dst_addr, orig_port, net_rule_list_offs);

	/* Lua:
	 *	  for i = 1, table.maxn(netruletable) do
	 *	        local rule = netruletable[i]
	*/
	for (i = 0; i < rule_list_size; i++) {
		ruletree_object_offset_t rule_offs;
		ruletree_net_rule_t	*rule;

                rule_offs = ruletree_objectlist_get_item(net_rule_list_offs, i);
		rule = offset_to_ruletree_object_ptr(rule_offs,
			SB2_RULETREE_OBJECT_TYPE_NET_RULE);

		if (!rule) {
			/* Usually the list does not have holes! */
			SB_LOG(SB_LOGLEVEL_NOISE,
				"%s: no rule @%d", __func__, rule_offs);
			continue;
		}

		/* Lua:
		 *	
		 *        if rule and rule.port and rule.port ~= orig_port then
		 *                rule = nil
		 *        end
		*/
		if (rule->rtree_net_port &&
		    (rule->rtree_net_port != orig_port)) {
			SB_LOG(SB_LOGLEVEL_NOISE,
				"%s: [%d] port does not match", __func__, i);
			continue;
		}

		/* Lua:
		 *        if rule and rule.func_name then
		 *                if rule.func_name == realfnname then
		 *                        sb.log("noise", "func_name ok in net_rule")
		 *                else
		 *                        rule = nil
		 *                end
		 *        end
		*/
		if (rule->rtree_net_func_name) {
			const char *fn = offset_to_ruletree_string_ptr(
				rule->rtree_net_func_name, NULL);
			if (!fn || !realfnname ||
			    strcmp(fn, realfnname)) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"%s: [%d] func_name does not match", __func__, i);
				continue;
			}
		}

		/* Lua:
		 *       if rule and rule.binary_name then
		 *              if rule.binary_name == realfnname then
		 *                      sb.log("noise", "binary_name ok in net_rule")
		 *              else
		 *                      rule = nil
		 *              end
		 *       end
		*/
		if (rule->rtree_net_binary_name) {
			const char *bn = offset_to_ruletree_string_ptr(
				rule->rtree_net_binary_name, NULL);
			if (!bn || !binary_name ||
			    strcmp(bn, binary_name)) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"%s: [%d] binary_name does not match", __func__, i);
				continue;
			}
		}
		
		/* Lua:
		 *        if rule and rule.address then
		 *                res,msg = sb.test_net_addr_match(addr_type,
		 *                        orig_dst_addr, rule.address)
		 *                if res then
		 *                        sb.log("noise", "address ok in net_rule, "..msg)
		 *                else
		 *                        sb.log("noise", "address test failed; "..msg)
		 *                        rule = nil
		 *                end
		 *          end
		*/
		if (rule->rtree_net_address) {
			const char *addr = offset_to_ruletree_string_ptr(
				rule->rtree_net_address, NULL);

			if (!addr ||
			    !test_net_addr_match(addr_type, orig_dst_addr, addr)) {
				SB_LOG(SB_LOGLEVEL_NOISE,
					"%s: [%d] addr. does not match", __func__, i);
				continue;
			}
		}

		/* Lua:
		 *          if rule and rule.rules then
		 *                return find_net_rule(rule.rules, realfnname, addr_type,
		 *                        orig_dst_addr, orig_port, binary_name)
		 *          end
		*/
		if (rule->rtree_net_rules) {
			SB_LOG(SB_LOGLEVEL_NOISE,
				"%s: [%d] => more rules @%d", __func__, rule->rtree_net_rules);
			return (find_net_rule(rule->rtree_net_rules, realfnname,
				addr_type, orig_dst_addr, orig_port, binary_name));
		}
		
		/* Lua:
		 *          if rule then
		 *                return rule
		 *          end
		*/
		SB_LOG(SB_LOGLEVEL_NOISE,
			"%s: rule found @%d", __func__, rule_offs);
		return(rule);
	}
	/* Lua:
	 *        end
	*/
	SB_LOG(SB_LOGLEVEL_NOISE, "%s: rule NOT found", __func__);
	return(NULL);
}

/* Body of this function was converted from (Lua) function
 *  sbox_map_network_addr()
 * returns zero if OK, or code for errno
*/
static int apply_net_rule(
	ruletree_net_rule_t *rule,
	char *result_addr_buf,
	int result_addr_buf_len,
	int *result_port)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "%s", __func__);
	
	/*	if rule.new_port then
	 *		dst_port = rule.new_port
	 *		sb.log("debug", "network port set to "..
	 *			dst_port.." (was "..orig_port..")")
	 *	end
	*/
	if (rule->rtree_net_new_port) {
		*result_port = rule->rtree_net_new_port;
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: port = %d",
			__func__, rule->rtree_net_new_port);
	}
	
	/*	if rule.new_address then
	 *		dst_addr = rule.new_address
	 *		sb.log("debug", "network addr set to "..
	 *			dst_addr.." (was "..orig_dst_addr..")")
	 *	end
	*/
	if (rule->rtree_net_new_address) {
		const char *n_addr = offset_to_ruletree_string_ptr(
				rule->rtree_net_new_address, NULL);
		if (n_addr) {
			SB_LOG(SB_LOGLEVEL_NOISE, "%s: addr = %s",
				__func__, n_addr);
			strncpy(result_addr_buf, n_addr, result_addr_buf_len);
		}
	}

	/*	if rule.allow then
	 *		if rule.deny then
	 *			sb.log("error", "network rule has both 'allow' and 'deny'")
	 *			return "EPERM", orig_dst_addr, orig_port,
	 *				rule.log_level, rule.log_msg
	 *		end
	 *		return "OK", dst_addr, dst_port, 
	 *				rule.log_level, rule.log_msg
	 *	else
	 *		if not rule.deny then
	 *			sb.log("error", "network rule must define either 'allow' or"..
	 *				" 'deny', this rule has neither")
	 *			return "EPERM", orig_dst_addr, orig_port,
	 *				rule.log_level, rule.log_msg
	 *		end
	 *		return "ENETUNREACH", orig_dst_addr, orig_port,
	 *				rule.log_level, rule.log_msg
	 *	end
	*/
	/* here ruletype must be either ALLOW or DENY, the
	 * third alternative (RULES) was handled in find_rule */
	switch (rule->rtree_net_ruletype) {
	case SB2_RULETREE_NET_RULETYPE_DENY:
		if (rule->rtree_net_errno) {
			SB_LOG(SB_LOGLEVEL_NOISE, "%s: deny; errno = %d",
				__func__, rule->rtree_net_errno);
			return(rule->rtree_net_errno);
		}
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: deny; errno defaults to EPERM",
			__func__);
		return(EPERM);
	case SB2_RULETREE_NET_RULETYPE_ALLOW:
		SB_LOG(SB_LOGLEVEL_NOISE, "%s: allowed.", __func__);
		return(0);
	}
	SB_LOG(SB_LOGLEVEL_ERROR, "%s: internal error: Unknown ruletype = %d",
		__func__, rule->rtree_net_ruletype);
	return(EPERM);
}

/* Returns:
 *  - nonzero: value for errno, result_addr_buf and *result_port 
 *    contain unknown values
 *  - zero: address was mapped, results in result_addr_buf and *result_port
*/
int sb2_map_network_addr(
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
	int result = 0;
	ruletree_net_rule_t *rule = NULL;
	const char *v[4];
	ruletree_object_offset_t	net_rule_list_offs;
	const char *modename = sbox_network_mode;

#if 1
	(void)protocol;
	(void)result_addr_buf_len;
#endif
	SB_LOG(SB_LOGLEVEL_NOISE, "sb2_map_network_addr for %s:%d (%s)",
		orig_dst_addr, orig_port, addr_type);
	SB_LOG(SB_LOGLEVEL_NOISE, "binary_name = '%s', fn=%s", binary_name, realfnname);

	if (!modename) {
		modename = ruletree_catalog_get_string("NET_RULES", "#default");
		if (!modename) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"failed to determine default network ruleset name (%s,function=%s)",
				binary_name, realfnname);
			return(EPERM);
		}
	}

	v[0] = "NET_RULES";
	v[1] = modename;
	v[2] = addr_type;
	v[3] = NULL;
	net_rule_list_offs = ruletree_catalog_vget(v);
	SB_LOG(SB_LOGLEVEL_NOISE, "%s: net rules at = %d", __func__, net_rule_list_offs);

	rule = find_net_rule(net_rule_list_offs, realfnname, addr_type,
		orig_dst_addr, orig_port, binary_name);

	result = EPERM; /* default value */
	if (rule) {
		strncpy(result_addr_buf, orig_dst_addr, result_addr_buf_len); /*fill default*/
		*result_port = orig_port; /* set default value */
		result = apply_net_rule(rule, result_addr_buf, result_addr_buf_len, result_port);
	}

	if (!result) {
		/* success */
		SB_LOG(SB_LOGLEVEL_NOISE, "sb2_map_network_addr => OK, addr=%s, port=%d",
			result_addr_buf, *result_port);
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE, "sb2_map_network_addr => ERROR %d",
			result);
	}

	return(result);
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
	res = sb2_map_network_addr(
		binary_name, fn_name, protocol,
		addr_type, dst_addr, port,
		result_buf, sizeof(result_buf),
		new_portp);
	*addr_bufp = strdup(result_buf);
	return(res);
}

