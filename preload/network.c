/*
 * network.c -- Network API of the scratchbox2 preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 * Copyright (C) 2010,2011 Nokia Corporation.
 *	Author: Lauri T. Aarnio
*/

/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libsb2.h"
#include "sb2_network.h"
#include "exported.h"

/* ---------- internal functions etc. ---------- */

typedef struct {
	socklen_t	mapped_addrlen;
	union	{
		struct sockaddr		mapped_sockaddr;
		struct sockaddr_in	mapped_sockaddr_in;
		struct sockaddr_in6	mapped_sockaddr_in6;
		struct sockaddr_un	mapped_sockaddr_un;
	};
	char	orig_printable_dst_addr[200];
	char	mapped_printable_dst_addr[200];
} mapped_sockaddr_t;

static void map_sockaddr_un(
	const char *realfnname,
	const struct sockaddr_un *orig_serv_addr_un,
	mapped_sockaddr_t *output_addr)
{
	mapping_results_t	res;

	if (!*orig_serv_addr_un->sun_path) {
		/* an "abstract" local domain socket.
		 * This is a Linux-specific extension */
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: abstract AF_UNIX addr",
			realfnname);
		output_addr->mapped_sockaddr_un = *orig_serv_addr_un;
		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"<abstract AF_UNIX address>");
		snprintf(output_addr->mapped_printable_dst_addr,
			sizeof(output_addr->mapped_printable_dst_addr),
			"<abstract AF_UNIX address>");
		return;
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: checking AF_UNIX addr '%s'",
		realfnname, orig_serv_addr_un->sun_path);

	snprintf(output_addr->orig_printable_dst_addr,
		sizeof(output_addr->orig_printable_dst_addr),
		"AF_UNIX %s", orig_serv_addr_un->sun_path);

	clear_mapping_results_struct(&res);
	/* FIXME: implement if(pathname_is_readonly!=0)... */
	sbox_map_path(realfnname, orig_serv_addr_un->sun_path,
		0/*dont_resolve_final_symlink*/, &res, SB2_INTERFACE_CLASS_SOCKADDR);
	if (res.mres_result_path == NULL) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: Failed to map AF_UNIX address '%s'",
			realfnname, orig_serv_addr_un->sun_path);
		snprintf(output_addr->mapped_printable_dst_addr,
			sizeof(output_addr->mapped_printable_dst_addr),
			"<Failed to map AF_UNIX address>");
	} else {
		output_addr->mapped_sockaddr_un = *orig_serv_addr_un;
		if (sizeof(output_addr->mapped_sockaddr_un.sun_path) <=
		    strlen(res.mres_result_path)) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Mapped AF_UNIX address (%s) is too long",
				realfnname, res.mres_result_path);
			snprintf(output_addr->mapped_printable_dst_addr,
				sizeof(output_addr->mapped_printable_dst_addr),
				"<Mapped AF_UNIX address is too long>");
		} else {
			strcpy(output_addr->mapped_sockaddr_un.sun_path,
				res.mres_result_path);
			output_addr->mapped_addrlen =
				offsetof(struct sockaddr_un, sun_path)
				 + strlen(res.mres_result_path) + 1;
			snprintf(output_addr->mapped_printable_dst_addr,
				sizeof(output_addr->mapped_printable_dst_addr),
				"AF_UNIX %s", res.mres_result_path);
		}
	}
	free_mapping_results(&res);
}

/* returns 0 if success, errno code if failed */
static int map_sockaddr_in(
	const char *realfnname,
	struct sockaddr_in *orig_sockaddr_in,
	socklen_t orig_addrlen,
	mapped_sockaddr_t *output_addr,
	const char *addr_type/* ipv4_{in,out} */)
{
	char printable_dst_addr[200];

	if (!orig_sockaddr_in) return(EFAULT);

	if (inet_ntop(AF_INET, &(orig_sockaddr_in->sin_addr),
		printable_dst_addr, sizeof(printable_dst_addr))) {

		char mapped_dst_addr[200];
		int mapping_result_code;
		int mapped_port;

		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"AF_INET %s:%d", printable_dst_addr,
			ntohs(orig_sockaddr_in->sin_port));
		/* Call mapping/filtering code: */
		*mapped_dst_addr = '\0';

		/* call the mapping logic. Parameter "protocol" is currently
		 * not used, because library calls like "connect()" do not
		 * specify it - it has been defined earlier. SB2 should
		 * either keep a table (cache) of socket types, or use
		 * syscalls to dig it out. This has not been implemented.
		 * To Be Fixed, if it turns out to be a real problem,
		 * currently it isn't. We have been able to live with a
		 * common set of protocol-independent rules. FIXME.
		*/
		mapping_result_code = sb2_map_network_addr(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			realfnname, NULL/*protocol. unknown. FIXME */,
			addr_type, printable_dst_addr,
			ntohs(orig_sockaddr_in->sin_port),
			mapped_dst_addr, sizeof(mapped_dst_addr),
			&mapped_port);

		if (mapping_result_code) return(mapping_result_code); /* errno-code */

		if (!*mapped_dst_addr) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: failed to map IPv4 address (internal error)",
				realfnname);
		} else {
			struct in_addr ina;

			switch(inet_pton(AF_INET, mapped_dst_addr, (void*)&ina)) {
			case 1: /* success */
				output_addr->mapped_addrlen = sizeof(struct sockaddr_in);
				output_addr->mapped_sockaddr_in.sin_port = htons(mapped_port);
				output_addr->mapped_sockaddr_in.sin_addr = ina;
				output_addr->mapped_sockaddr.sa_family = AF_INET;
				
				snprintf(output_addr->mapped_printable_dst_addr,
					sizeof(output_addr->mapped_printable_dst_addr),
					"AF_INET %s:%d", mapped_dst_addr, mapped_port);
				return(0); /* ok to use this address */
			case 0: 
				SB_LOG(SB_LOGLEVEL_ERROR,
					"%s: IPv4 address mapping returned an "
					"illegal string (inet_pton() can't convert it;"
					" this is an internal error)",
					realfnname);
				break;
			case -1: /* EAFNOSUPPORT */
			default:
				SB_LOG(SB_LOGLEVEL_ERROR,
					"%s: IPv4 address conversion error in "
					"inet_pton() (internal error)",
					realfnname);
				break;
			}
			/* allow use of the orig.address because inet_pton() failed */
			snprintf(output_addr->mapped_printable_dst_addr,
				sizeof(output_addr->mapped_printable_dst_addr),
				"%s", output_addr->orig_printable_dst_addr);
		}
	} else {
		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"<AF_INET address conversion failed>");
		snprintf(output_addr->mapped_printable_dst_addr,
			sizeof(output_addr->mapped_printable_dst_addr),
			"<AF_INET address conversion failed>");
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: failed to convert IPv4 address to string",
			realfnname);
	}

	/* Use orig.address */
	output_addr->mapped_sockaddr_in = *orig_sockaddr_in;
	output_addr->mapped_addrlen = orig_addrlen;
	return 0;
}

/* returns 0 if success, errno code if failed */
static int map_sockaddr_in6(
	const char *realfnname,
	struct sockaddr_in6 *orig_sockaddr_in6,
	socklen_t orig_addrlen,
	mapped_sockaddr_t *output_addr,
	const char *addr_type/* ipv6_{in,out} */)
{
	char printable_dst_addr[200];

	if (!orig_sockaddr_in6) return(EFAULT);

	if (inet_ntop(AF_INET6, &(orig_sockaddr_in6->sin6_addr),
		printable_dst_addr, sizeof(printable_dst_addr))) {

		char mapped_dst_addr[200];
		int mapping_result_code;
		int mapped_port;

		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"AF_INET6 [%s]:%d", printable_dst_addr,
			ntohs(orig_sockaddr_in6->sin6_port));
		/* Call mapping/filtering code: */
		*mapped_dst_addr = '\0';

		/* call the mapping logic. Parameter "protocol" is currently
		 * not used, because library calls like "connect()" do not
		 * specify it - it has been defined earlier. SB2 should
		 * either keep a table (cache) of socket types, or use
		 * syscalls to dig it out. This has not been implemented.
		 * To Be Fixed, if it turns out to be a real problem,
		 * currently it isn't. We have been able to live with a
		 * common set of protocol-independent rules. FIXME.
		*/
		mapping_result_code = sb2_map_network_addr(
			(sbox_binary_name ? sbox_binary_name : "UNKNOWN"),
			realfnname, NULL/*protocol. unknown. FIXME */,
			addr_type, printable_dst_addr,
			ntohs(orig_sockaddr_in6->sin6_port),
			mapped_dst_addr, sizeof(mapped_dst_addr),
			&mapped_port);

		if (mapping_result_code) return(mapping_result_code); /* errno-code */

		if (!*mapped_dst_addr) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: failed to map IPv6 address (internal error)",
				realfnname);
		} else {
			struct in6_addr ina6;

			switch(inet_pton(AF_INET6, mapped_dst_addr, (void*)&ina6)) {
			case 1: /* success */
				output_addr->mapped_addrlen = sizeof(struct sockaddr_in6);
				output_addr->mapped_sockaddr_in6.sin6_port = htons(mapped_port);
				output_addr->mapped_sockaddr_in6.sin6_addr = ina6;
				output_addr->mapped_sockaddr.sa_family = AF_INET6;
				
				snprintf(output_addr->mapped_printable_dst_addr,
					sizeof(output_addr->mapped_printable_dst_addr),
					"AF_INET6 [%s]:%d", mapped_dst_addr, mapped_port);
				return(0); /* ok to use this address */
			case 0: 
				SB_LOG(SB_LOGLEVEL_ERROR,
					"%s: IPv6 address mapping returned an "
					"illegal string (inet_pton() can't convert it;"
					" this is an internal error)",
					realfnname);
				break;
			case -1: /* EAFNOSUPPORT */
			default:
				SB_LOG(SB_LOGLEVEL_ERROR,
					"%s: IPv6 address conversion error in "
					"inet_pton() (internal error)",
					realfnname);
				break;
			}
			/* allow use of the orig.address because inet_pton() failed */
			snprintf(output_addr->mapped_printable_dst_addr,
				sizeof(output_addr->mapped_printable_dst_addr),
				"%s", output_addr->orig_printable_dst_addr);
		}
	} else {
		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"<AF_INET6 address conversion failed>");
		snprintf(output_addr->mapped_printable_dst_addr,
			sizeof(output_addr->mapped_printable_dst_addr),
			"<AF_INET6 address conversion failed>");
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: failed to convert IPv6 address to string",
			realfnname);
	}

	/* Use orig.address */
	output_addr->mapped_sockaddr_in6 = *orig_sockaddr_in6;
	output_addr->mapped_addrlen = orig_addrlen;
	return 0;
}

static void log_mapped_net_op_result(
	const char *realfnname,
	int result_errno,
	mapped_sockaddr_t *addr)
{
	char *result_str = "OK";

	if (result_errno) switch (result_errno) {
	case EADDRNOTAVAIL: result_str = "addr.not available"; break;
	case EINPROGRESS: result_str = "in progress"; break;
	case EPERM: result_str = "EPERM"; break;
	default: result_str = "Failed"; break;
	}
	SB_LOG(SB_LOGLEVEL_NETWORK, "%s: %s => %s (%d)",
		realfnname, addr->mapped_printable_dst_addr,
		result_str, result_errno);
}

/* map_sockaddr() returns one of: */
#define MAP_SOCKADDR_OPERATION_DENIED	(-1) /* ..if operation is denied (errno set) */
#define MAP_SOCKADDR_MAPPED		(0)  /* ..call the real function with "output_addr" */
#define MAP_SOCKADDR_USE_ORIG_ADDR	(1)  /* ..not mapped. */
static int map_sockaddr(
	int *result_errno_ptr,
	const char *realfnname,
	const struct sockaddr *input_addr,
	socklen_t input_addrlen,
	mapped_sockaddr_t *output_addr,
	const char *direction) /* "in" or "out", uset to build
				* e.g. "ipv4_in", "ipv4_out",.. */
{
	int	inet_mapping_result;
	char	addr_type[100];

	memset(output_addr, 0, sizeof(*output_addr));
	if (input_addr) {
		switch (input_addr->sa_family) {
		case AF_UNIX:
			output_addr->mapped_addrlen = input_addrlen;

			map_sockaddr_un(realfnname,
				(const struct sockaddr_un*)input_addr,
				output_addr);
			SB_LOG(SB_LOGLEVEL_DEBUG, "%s: orig addr.len=%d, mapped_addrlen=%d",
				__func__, input_addrlen, output_addr->mapped_addrlen);
			return (MAP_SOCKADDR_MAPPED);

		case AF_INET:
			snprintf(addr_type, sizeof(addr_type), "ipv4_%s", direction);
			inet_mapping_result = map_sockaddr_in(realfnname,
				(struct sockaddr_in*)input_addr, input_addrlen,
				output_addr, addr_type);
			goto check_inet_mapping_result;

		case AF_INET6:
			snprintf(addr_type, sizeof(addr_type), "ipv6_%s", direction);
			inet_mapping_result = map_sockaddr_in6(realfnname,
				(struct sockaddr_in6*)input_addr, input_addrlen,
				output_addr, addr_type);
			goto check_inet_mapping_result;

		default:
			snprintf(output_addr->orig_printable_dst_addr,
				sizeof(output_addr->orig_printable_dst_addr),
				"<Network mapping does not support this AF=%d, uses orig.address>",
				input_addr->sa_family);
			break;
		}
	} else {
		snprintf(output_addr->orig_printable_dst_addr,
			sizeof(output_addr->orig_printable_dst_addr),
			"<no address>");
	}
	snprintf(output_addr->mapped_printable_dst_addr,
		sizeof(output_addr->mapped_printable_dst_addr),
		"%s", output_addr->orig_printable_dst_addr);
	return(MAP_SOCKADDR_USE_ORIG_ADDR);

    check_inet_mapping_result:
	/* IPv4 or IPv6 address was processed: */
	if (inet_mapping_result != 0) {
		/* return error */
		*result_errno_ptr = inet_mapping_result;
		SB_LOG(SB_LOGLEVEL_NETWORK,
			"%s: denied (%s), errno=%d",
			realfnname, output_addr->orig_printable_dst_addr,
			inet_mapping_result);
		return(MAP_SOCKADDR_OPERATION_DENIED);
	}
	if (strcmp(output_addr->orig_printable_dst_addr,
	    output_addr->mapped_printable_dst_addr)) {
		SB_LOG(SB_LOGLEVEL_NETWORK,
			"%s: allowed, address changed "
			"(orig.addr=%s, new addr=%s)", realfnname,
			output_addr->orig_printable_dst_addr,
			output_addr->mapped_printable_dst_addr);
	} else {
		SB_LOG(SB_LOGLEVEL_NETWORK,
			"%s: allowed (%s)", realfnname,
			output_addr->orig_printable_dst_addr);
	}
	return(MAP_SOCKADDR_MAPPED);
}

/* ---------- Socket API ---------- */

int bind_gate(
	int *result_errno_ptr,
	int (*real_bind_ptr)(int sockfd, const struct sockaddr *my_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *my_addr,
	socklen_t addrlen)
{
	int			result;
	mapped_sockaddr_t	mapped_addr;

	switch (map_sockaddr(result_errno_ptr, realfnname,
		my_addr, addrlen, &mapped_addr, "in")) {
	case MAP_SOCKADDR_OPERATION_DENIED:
		return (-1);
	case MAP_SOCKADDR_MAPPED:
		errno = *result_errno_ptr; /* restore to orig.value */
		result = (*real_bind_ptr)(sockfd,
			&mapped_addr.mapped_sockaddr,
			mapped_addr.mapped_addrlen);
		*result_errno_ptr = errno;
		log_mapped_net_op_result(realfnname, (result?errno:0),
			&mapped_addr);
		return(result);
	case MAP_SOCKADDR_USE_ORIG_ADDR:
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: internal error: unknown result code from map_sockaddr()",
			realfnname);
	}
	errno = *result_errno_ptr; /* restore to orig.value */
	result = (*real_bind_ptr)(sockfd, my_addr, addrlen);
	*result_errno_ptr = errno;
	return(result);
}

int connect_gate(
	int *result_errno_ptr,
	int (*real_connect_ptr)(int sockfd, const struct sockaddr *serv_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *serv_addr,
	socklen_t addrlen)
{
	int			result;
	mapped_sockaddr_t	mapped_addr;

	switch (map_sockaddr(result_errno_ptr, realfnname,
		serv_addr, addrlen, &mapped_addr, "out")) {
	case MAP_SOCKADDR_OPERATION_DENIED:
		return (-1);
	case MAP_SOCKADDR_MAPPED:
		errno = *result_errno_ptr; /* restore to orig.value */
		result = (*real_connect_ptr)(sockfd,
			&mapped_addr.mapped_sockaddr,
			mapped_addr.mapped_addrlen);
		*result_errno_ptr = errno;
		log_mapped_net_op_result(realfnname, (result?errno:0),
			&mapped_addr);
		return(result);
	case MAP_SOCKADDR_USE_ORIG_ADDR:
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: internal error: unknown result code from map_sockaddr()",
			realfnname);
	}
	errno = *result_errno_ptr; /* restore to orig.value */
	result = (*real_connect_ptr)(sockfd, serv_addr, addrlen);
	*result_errno_ptr = errno;
	return(result);
}

ssize_t sendto_gate(
	int *result_errno_ptr,
	ssize_t (*real_sendto_ptr)(int s, const void *buf,
	size_t len, int flags, const struct sockaddr *to, socklen_t tolen),
        const char *realfnname,
	int s,
	const void *buf,
	size_t len,
	int flags,
	const struct sockaddr *to,
	socklen_t tolen)
{
	ssize_t	result;
	mapped_sockaddr_t	mapped_addr;

	/* FIXME: If the socket is connected (SOCK_STREAM, SOCK_SEQPACKET)
	 * "to" is ignored and we should not try to map it. */

	switch (map_sockaddr(result_errno_ptr, realfnname,
		to, tolen, &mapped_addr, "out")) {
	case MAP_SOCKADDR_OPERATION_DENIED:
		return (-1);
	case MAP_SOCKADDR_MAPPED:
		errno = *result_errno_ptr; /* restore to orig.value */
		result = (*real_sendto_ptr)(s, buf,
			len, flags,
			&mapped_addr.mapped_sockaddr,
			mapped_addr.mapped_addrlen);
		*result_errno_ptr = errno;
		log_mapped_net_op_result(realfnname, (result==-1?errno:0),
			&mapped_addr);
		return(result);
	case MAP_SOCKADDR_USE_ORIG_ADDR:
		break;
	default:
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: internal error: unknown result code from map_sockaddr()",
			realfnname);
	}
	errno = *result_errno_ptr; /* restore to orig.value */
	result = (*real_sendto_ptr)(s, buf, len, flags, to, tolen);
	*result_errno_ptr = errno;
	return(result);
}

ssize_t sendmsg_gate(
	int *result_errno_ptr,
	ssize_t (*real_sendmsg_ptr)(int s,
	const struct msghdr *msg, int flags),
        const char *realfnname,
	int s,
	const struct msghdr *msg,
	int flags)
{
	ssize_t	result;

	/* FIXME: see the comment about connected sockets in sendto_gate() */

	if (msg) {
		const struct sockaddr	*to = (struct sockaddr*)msg->msg_name;
		mapped_sockaddr_t	mapped_addr;
		struct msghdr		msg2 = *msg;

		switch (map_sockaddr(result_errno_ptr, realfnname,
			to, msg->msg_namelen, &mapped_addr, "out")) {
		case MAP_SOCKADDR_OPERATION_DENIED:
			return (-1);
		case MAP_SOCKADDR_MAPPED:
			msg2.msg_name = &mapped_addr.mapped_sockaddr;
			msg2.msg_namelen = mapped_addr.mapped_addrlen;
			errno = *result_errno_ptr; /* restore to orig.value */
			result = (*real_sendmsg_ptr)(s, &msg2, flags);
			*result_errno_ptr = errno;
			log_mapped_net_op_result(realfnname, (result==-1?errno:0),
				&mapped_addr);
			return(result);
		case MAP_SOCKADDR_USE_ORIG_ADDR:
			break;
		default:
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: internal error: unknown result code from map_sockaddr()",
				realfnname);
		}
	}
	errno = *result_errno_ptr; /* restore to orig.value */
	result = (*real_sendmsg_ptr)(s, msg, flags);
	*result_errno_ptr = errno;
	return(result);
}

static void reverse_sockaddr_un(
	const char *realfnname,
	struct sockaddr *from,
	socklen_t orig_from_size,
	socklen_t *fromlen)
{
	struct sockaddr_un *from_un;
	char *sbox_path = NULL;

	if (!from || !fromlen || (*fromlen < 1) || (orig_from_size < 1)) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			 "%s: nothing to reverse", realfnname);
		return;
	}

	if (from->sa_family != AF_UNIX) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			 "%s: not AF_UNIX", realfnname);
		return;
	}

	if (*fromlen <= offsetof(struct sockaddr_un, sun_path)) {
		/* empty address, nothing to reverse */
		SB_LOG(SB_LOGLEVEL_NOISE2,
			 "%s: empty AF_UNIX address", realfnname);
		return;
	}

	from_un = (struct sockaddr_un*)from;
	if (from_un->sun_path[0] == '\0') {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			 "%s: abstract AF_UNIX address", realfnname);
		return;
	}

	/* a non-abstract unix domain socket address, reverse it */
	sbox_path = scratchbox_reverse_path(realfnname, from_un->sun_path,
			SB2_INTERFACE_CLASS_SOCKADDR);
	if (sbox_path) {
		size_t max_path_size = orig_from_size -
			 offsetof(struct sockaddr_un, sun_path);

		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: reversed to '%s'",
			realfnname, sbox_path);
		if (strlen(sbox_path) >= max_path_size) {
			/* address does not fit */
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"%s: (result will be cut)", realfnname);
			strncpy(from_un->sun_path, sbox_path, max_path_size);
			*fromlen = orig_from_size;
		} else {
			strcpy(from_un->sun_path, sbox_path);
			*fromlen = offsetof(struct sockaddr_un, sun_path)
				+ strlen(sbox_path) + 1;
		}
		free(sbox_path);
	}
}

ssize_t recvfrom_gate(
	int *result_errno_ptr,
	ssize_t (*real_recvfrom_ptr)(int s, void *buf,
		size_t len, int flags, struct sockaddr *from, socklen_t *fromlen),
        const char *realfnname,
	int s,
	void *buf,
	size_t len,
	int flags,
	struct sockaddr *from,
	socklen_t *fromlen)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (fromlen) orig_from_size = *fromlen;
	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_recvfrom_ptr)(s, buf, len, flags, from, fromlen);
	*result_errno_ptr = errno;
	if (from) reverse_sockaddr_un(realfnname, from, orig_from_size, fromlen);
	return (res);
}

ssize_t __recvfrom_chk_gate(
	int *result_errno_ptr,
	ssize_t (*real___recvfrom_chk_ptr)(int s,
		void *__restrict buf, size_t __n, size_t __buflen, int flags,
		struct sockaddr *from, socklen_t *__restrict fromlen),
	const char *realfnname,
	int s,
	void *__restrict buf,
	size_t __n,
	size_t __buflen,
	int flags,
	struct sockaddr *from,
	socklen_t *__restrict fromlen)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (fromlen) orig_from_size = *fromlen;

	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real___recvfrom_chk_ptr)(s, buf, __n, __buflen, flags,
		from, fromlen);
	*result_errno_ptr = errno;
	if (from) reverse_sockaddr_un(realfnname, from, orig_from_size, fromlen);
	return (res);
}

ssize_t recvmsg_gate(
	int *result_errno_ptr,
	ssize_t (*real_recvmsg_ptr)(int s,
	struct msghdr *msg, int flags),
        const char *realfnname,
	int s,
	struct msghdr *msg,
	int flags)
{
	ssize_t	res;
	socklen_t orig_from_size = msg ? msg->msg_namelen : 0;

	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_recvmsg_ptr)(s, msg, flags);
	*result_errno_ptr = errno;
	if (msg && msg->msg_name) {
		reverse_sockaddr_un(realfnname, msg->msg_name,
			orig_from_size, &(msg->msg_namelen));
	}
	return (res);
}

int accept_gate(
	int *result_errno_ptr,
	int (*real_accept_ptr)(int sockfd,
	struct sockaddr *addr, socklen_t *addrlen),
        const char *realfnname,
	int sockfd,
	struct sockaddr *addr,
	socklen_t *addrlen)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (addrlen) orig_from_size = *addrlen;

	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_accept_ptr)(sockfd, addr, addrlen);
	*result_errno_ptr = errno;
	if (addr) reverse_sockaddr_un(realfnname, addr, orig_from_size, addrlen);
	return (res);
}

int accept4_gate(
	int *result_errno_ptr,
	int (*real_accept4_ptr)(int sockfd,
		struct sockaddr *addr, socklen_t *addrlen, int flags),
        const char *realfnname,
	int sockfd,
	struct sockaddr *addr,
	socklen_t *addrlen,
	int flags)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (addrlen) orig_from_size = *addrlen;

	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_accept4_ptr)(sockfd, addr, addrlen, flags);
	*result_errno_ptr = errno;
	if (addr) reverse_sockaddr_un(realfnname, addr, orig_from_size, addrlen);
	return (res);
}

int getpeername_gate(
	int *result_errno_ptr,
	int (*real_getpeername_ptr)(int s,
	struct sockaddr *name, socklen_t *namelen),
        const char *realfnname,
	int s,
	struct sockaddr *name,
	socklen_t *namelen)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (namelen) orig_from_size = *namelen;
	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_getpeername_ptr)(s, name, namelen);
	*result_errno_ptr = errno;
	if (name) reverse_sockaddr_un(realfnname, name, orig_from_size, namelen);
	return (res);
}

int getsockname_gate(
	int *result_errno_ptr,
	int (*real_getsockname_ptr)(int s,
	struct sockaddr *name, socklen_t *namelen),
        const char *realfnname,
	int s,
	struct sockaddr *name,
	socklen_t *namelen)
{
	ssize_t	res;
	socklen_t orig_from_size = 0;

	if (namelen) orig_from_size = *namelen;
	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_getsockname_ptr)(s, name, namelen);
	*result_errno_ptr = errno;
	if (name) reverse_sockaddr_un(realfnname, name, orig_from_size, namelen);
	return (res);
}

