/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* Rule tree client: A debugging tool, can be used to send commands to sb2d.
*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <assert.h>

#include "sb2.h"
#include "rule_tree.h"
#include "rule_tree_rpc.h"
#include "mapping.h"
#include "libsb2.h"

#include "exported.h"



static const char *progname = NULL;
char    *sbox_session_dir = NULL;

int ruletree_get_min_client_socket_fd(void)
{
	return(10);
}

int main(int argc, char *argv[])
{
	int	opt;
	const char	*cmd;

	progname = argv[0];

	/* init logging before doing anything else,
	 * otherwise the logger will auto-initialize itself.
	 * if debug_level and/or debug_file is NULL, logger
	 * will read the values from env.vars. */
	sblog_init_level_logfile_format("debug", "-", NULL);

	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
#if 0
		case 'd':
			debug = sb_loglevel__ = atoi(optarg);
			break;
#endif
		default:
			fprintf(stderr, "Illegal option\n");
			exit(1);
		}
	}

	sbox_session_dir = getenv("SBOX_SESSION_DIR");
	if (!sbox_session_dir) {
		fprintf(stderr, "ERROR: no session "
			"(SBOX_SESSION_DIR is not set) - this program"
			" must be used inside a Scratchbox 2 session\n");
		exit(1);
	}

	cmd = argv[optind];
	if (!cmd) {
		fprintf(stderr, "Usage:\n\t%s command\n", argv[0]);
		fprintf(stderr, "commands\n"
				"   ping     Send a 'ping' to sb2d, wait for reply\n");
		exit(1);
	}

	if (!strcmp(cmd, "ping")) {
		ruletree_rpc__ping();
	} else {
		fprintf(stderr, "Unknown command %s\n", cmd);
		exit(1);
	}

	return(0);
}

/* This program is directly linked to the RPC routines
 * (because they are hidden in libsb2, and cound not be used otherwise).
 * The RPC routines want some wrappers for *_nomap_nolog functions,
 * these will use the ordinary functions... a side-effect is that the 
 * network addresses are subject to mapping operations here. 
 * Fortunately that won't happen in the usual case when the RPC
 * functions are used inside libsb2.
*/
int unlink_nomap_nolog(const char *pathname)
{
	return(unlink(pathname));
}

ssize_t sendto_nomap_nolog(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	return(sendto(s, buf, len, flags, to, tolen));
}

ssize_t recvfrom_nomap_nolog(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	return(recvfrom(s, buf, len, flags, from, fromlen));
}

int bind_nomap_nolog(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen)
{
	return(bind(sockfd, my_addr, addrlen));
}

int chmod_nomap_nolog(const char *path, mode_t mode)
{
	return(chmod(path, mode));
}

