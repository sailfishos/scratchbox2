/*
 * network.c -- Network API of the scratchbox2 preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 *	Lauri T. Aarnio
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
#include "libsb2.h"
#include "exported.h"

/* ---------- Socket API ---------- */

static void map_sockaddr_un(
	const char *realfnname,
	const struct sockaddr_un *orig_serv_addr_un,
	struct sockaddr_un *mapped_serv_addr_un,
	socklen_t *new_addrlen)
{
	mapping_results_t	res;

	if (!*orig_serv_addr_un->sun_path) {
		/* an "abstract" local domain socket.
		 * This is an Linux extension */
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: abstract AF_UNIX addr",
			realfnname);
		*mapped_serv_addr_un = *orig_serv_addr_un;
		return;
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: checking AF_UNIX addr '%s'",
		realfnname, orig_serv_addr_un->sun_path);

	clear_mapping_results_struct(&res);
	/* FIXME: implement if(pathname_is_readonly!=0)... */
	sbox_map_path(realfnname, orig_serv_addr_un->sun_path,
		0/*dont_resolve_final_symlink*/, &res);
	if (res.mres_result_path == NULL) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"%s: Failed to map AF_UNIX address '%s'",
			realfnname, orig_serv_addr_un->sun_path);
	} else {
		*mapped_serv_addr_un = *orig_serv_addr_un;
		if (sizeof(mapped_serv_addr_un->sun_path) <=
		    strlen(res.mres_result_path)) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Mapped AF_UNIX address (%s) is too long",
				realfnname, res.mres_result_path);
		} else {
			strcpy(mapped_serv_addr_un->sun_path,
				res.mres_result_path);
			*new_addrlen = offsetof(struct sockaddr_un, sun_path)
				+ strlen(res.mres_result_path) + 1;
		}
	}
	free_mapping_results(&res);
}

int bind_gate(
	int (*real_bind_ptr)(int sockfd, const struct sockaddr *my_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *my_addr,
	socklen_t addrlen)
{
	if (my_addr && (my_addr->sa_family == AF_UNIX)) {
		struct sockaddr_un mapped_my_addr_un;
		socklen_t new_addrlen = addrlen;

		map_sockaddr_un(realfnname,
			(const struct sockaddr_un*)my_addr,
			&mapped_my_addr_un, &new_addrlen);

		return((*real_bind_ptr)(sockfd,
			(struct sockaddr*)&mapped_my_addr_un,
			new_addrlen));
	}
	return((*real_bind_ptr)(sockfd, my_addr, addrlen));
}

int connect_gate(
	int (*real_connect_ptr)(int sockfd, const struct sockaddr *serv_addr,
		socklen_t addrlen),
	const char *realfnname,
	int sockfd,
	const struct sockaddr *serv_addr,
	socklen_t addrlen)
{
	struct sockaddr_un mapped_serv_addr_un;
	socklen_t new_addrlen;
	char printable_dst_addr[200];
	int r;
	int saved_errno;
	struct sockaddr_in *ina;
	struct sockaddr_in6 *ina6;

	if (serv_addr) {
		switch (serv_addr->sa_family) {
		case AF_UNIX:
			new_addrlen = addrlen;

			map_sockaddr_un(realfnname,
				(const struct sockaddr_un*)serv_addr,
				&mapped_serv_addr_un, &new_addrlen);

			return((*real_connect_ptr)(sockfd,
				(struct sockaddr*)&mapped_serv_addr_un,
				new_addrlen));
		case AF_INET:
			ina = (struct sockaddr_in*)serv_addr;
			r = (*real_connect_ptr)(sockfd, serv_addr, addrlen);
			saved_errno = errno;
			if (inet_ntop(serv_addr->sa_family,
				&ina->sin_addr,
				printable_dst_addr, sizeof(printable_dst_addr))) {

				SB_LOG(SB_LOGLEVEL_NETWORK,
					"%s: %s:%d => %s", realfnname,
					printable_dst_addr, ina->sin_port,
					(r ? "Failed" : "OK"));
			} else {
				SB_LOG(SB_LOGLEVEL_NETWORK,
					"%s: <address conversion failed> => %s",
					realfnname,
					(r ? "Failed" : "OK"));
			}
			errno = saved_errno;
			return(r);
		case AF_INET6:
			ina6 = (struct sockaddr_in6*)serv_addr;
			r = (*real_connect_ptr)(sockfd, serv_addr, addrlen);
			saved_errno = errno;
			if (inet_ntop(serv_addr->sa_family,
				&ina6->sin6_addr,
				printable_dst_addr, sizeof(printable_dst_addr))) {

				SB_LOG(SB_LOGLEVEL_NETWORK,
					"%s: [%s]:%d => %s", realfnname,
					printable_dst_addr, ina6->sin6_port,
					(r ? "Failed" : "OK"));
			} else {
				SB_LOG(SB_LOGLEVEL_NETWORK,
					"%s: <address conversion failed> => %s",
					realfnname,
					(r ? "Failed" : "OK"));
			}
			errno = saved_errno;
			return(r);
		}
	}
	return((*real_connect_ptr)(sockfd, serv_addr, addrlen));
}

ssize_t sendto_gate(ssize_t (*real_sendto_ptr)(int s, const void *buf,
	size_t len, int flags, const struct sockaddr *to, socklen_t tolen),
        const char *realfnname,
	int s,
	const void *buf,
	size_t len,
	int flags,
	const struct sockaddr *to,
	socklen_t tolen)
{
	if (to && (to->sa_family == AF_UNIX)) {
		struct sockaddr_un mapped_to_addr;
		socklen_t new_addrlen = tolen;

		map_sockaddr_un(realfnname,
			(const struct sockaddr_un*)to,
			&mapped_to_addr, &new_addrlen);

		return((*real_sendto_ptr)(s, buf,
			len, flags, (struct sockaddr*)&mapped_to_addr, 
			new_addrlen));
	}
	return((*real_sendto_ptr)(s, buf, len, flags, to, tolen));
}

ssize_t sendmsg_gate(ssize_t (*real_sendmsg_ptr)(int s,
	const struct msghdr *msg, int flags),
        const char *realfnname,
	int s,
	const struct msghdr *msg,
	int flags)
{
	if (msg) {
		const struct sockaddr *to = (struct sockaddr*)msg->msg_name;
		if (to && (to->sa_family == AF_UNIX)) {
			struct sockaddr_un mapped_to_addr;
			struct msghdr msg2 = *msg;
			socklen_t new_addrlen = msg->msg_namelen;

			map_sockaddr_un(realfnname,
				(const struct sockaddr_un*)to,
				&mapped_to_addr, &new_addrlen);
			msg2.msg_name = &mapped_to_addr;
			msg2.msg_namelen = new_addrlen;
			return((*real_sendmsg_ptr)(s, &msg2, flags));
		}
	}
	return((*real_sendmsg_ptr)(s, msg, flags));
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
	sbox_path = scratchbox_reverse_path(realfnname, from_un->sun_path);
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

ssize_t recvfrom_gate(ssize_t (*real_recvfrom_ptr)(int s, void *buf,
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
	socklen_t orig_from_size = *fromlen;

	res = (*real_recvfrom_ptr)(s, buf, len, flags, from, fromlen);
	reverse_sockaddr_un(realfnname, from, orig_from_size, fromlen);
	return (res);
}

ssize_t __recvfrom_chk_gate(ssize_t (*real___recvfrom_chk_ptr)(int s,
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
	socklen_t orig_from_size = *fromlen;

	res = (*real___recvfrom_chk_ptr)(s, buf, __n, __buflen, flags,
		from, fromlen);
	reverse_sockaddr_un(realfnname, from, orig_from_size, fromlen);
	return (res);
}

ssize_t recvmsg_gate(ssize_t (*real_recvmsg_ptr)(int s,
	struct msghdr *msg, int flags),
        const char *realfnname,
	int s,
	struct msghdr *msg,
	int flags)
{
	ssize_t	res;
	socklen_t orig_from_size = msg ? msg->msg_namelen : 0;

	res = (*real_recvmsg_ptr)(s, msg, flags);
	if (msg && msg->msg_name) {
		reverse_sockaddr_un(realfnname, msg->msg_name,
			orig_from_size, &(msg->msg_namelen));
	}
	return (res);
}

int accept_gate(int (*real_accept_ptr)(int sockfd,
	struct sockaddr *addr, socklen_t *addrlen),
        const char *realfnname,
	int sockfd,
	struct sockaddr *addr,
	socklen_t *addrlen)
{
	ssize_t	res;
	socklen_t orig_from_size = *addrlen;

	res = (*real_accept_ptr)(sockfd, addr, addrlen);
	reverse_sockaddr_un(realfnname, addr, orig_from_size, addrlen);
	return (res);
}

int getpeername_gate(int (*real_getpeername_ptr)(int s,
	struct sockaddr *name, socklen_t *namelen),
        const char *realfnname,
	int s,
	struct sockaddr *name,
	socklen_t *namelen)
{
	ssize_t	res;
	socklen_t orig_from_size = *namelen;

	res = (*real_getpeername_ptr)(s, name, namelen);
	reverse_sockaddr_un(realfnname, name, orig_from_size, namelen);
	return (res);
}

int getsockname_gate(int (*real_getsockname_ptr)(int s,
	struct sockaddr *name, socklen_t *namelen),
        const char *realfnname,
	int s,
	struct sockaddr *name,
	socklen_t *namelen)
{
	ssize_t	res;
	socklen_t orig_from_size = *namelen;

	res = (*real_getsockname_ptr)(s, name, namelen);
	reverse_sockaddr_un(realfnname, name, orig_from_size, namelen);
	return (res);
}

