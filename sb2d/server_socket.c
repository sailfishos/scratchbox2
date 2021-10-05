/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * License: LGPL-2.1
*/

/* Rule tree server, server socket routines */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/inotify.h>

#include "sb2_server.h"

static struct sockaddr_un server_address;
static socklen_t	server_addr_len;
static int server_socket = -1;
static int inotify_fd = -1;
static int inotify_server_sock_dir_wd = -1;
static char *server_sock_dir = NULL;

static void initialize_server_address(void)
{
	char	*sock_path = NULL;
	size_t	sock_path_len;

	if (asprintf(&server_sock_dir, "%s/sb2d-sock.d", sbox_session_dir) < 0) {
		fprintf(stderr, "%s: Fatal: asprintf failed\n", progname);
		exit(1);
	}
	if ((mkdir(server_sock_dir, 0700) < -1) && (errno != EEXIST)) {
		fprintf(stderr, "%s: failed to create directory %s\n",
			progname, server_sock_dir);
		exit(1);
	}
	if (asprintf(&sock_path, "%s/ssock", server_sock_dir) < 0) {
		fprintf(stderr, "%s: Fatal: asprintf failed\n", progname);
		exit(1);
	}
	sock_path_len = strlen(sock_path);
	if (sock_path_len >= sizeof(server_address.sun_path)-1) {
		fprintf(stderr, "%s: Fatal: server socket address lenght is too big (%d)\n",
			progname, (int)sock_path_len);
		exit(1);
	}
	server_address.sun_family = AF_UNIX;
	strcpy(server_address.sun_path, sock_path); /* it fits. */
	server_addr_len = sizeof(sa_family_t) + sock_path_len + 1;
	free(sock_path);
}

void create_server_socket(void)
{
	server_socket = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (server_socket < 0) {
		fprintf(stderr, "%s: Fatal: Failed to create server socket\n", progname);
		exit(1);
	}
	initialize_server_address();
	unlink(server_address.sun_path); /* remove old socket, if any. */

	if (bind(server_socket, (struct sockaddr*)&server_address, server_addr_len) < 0) {
		fprintf(stderr, "%s: Fatal: Failed to bind server socket address (%s)\n",
			progname, server_address.sun_path);
		exit(1);
	}
	SB_LOG(SB_LOGLEVEL_DEBUG, "server socket = (%s)", server_address.sun_path);
	/* done, ok. */

	/* we'll use inotify to watch if the socket gets removed.
	 * that is why a subdir was created for the socket (there should
	 * not be anything else in that subdir):
	 * If a delete event arrives, it is most probably related removal
	 * of server_socket */
	inotify_fd = inotify_init();
	inotify_server_sock_dir_wd = inotify_add_watch(inotify_fd,
		server_sock_dir, IN_DELETE);
	SB_LOG(SB_LOGLEVEL_DEBUG, "inotify_fd = %d, inotify_server_sock_dir_wd = %d",
		inotify_fd, inotify_server_sock_dir_wd);
}

void send_reply_to_client(struct sockaddr_un *client_address,
	ruletree_rpc_msg_reply_t *reply,
	size_t reply_size)
{
	ssize_t	sent_msg_size;

	sent_msg_size = sendto(server_socket, reply, reply_size, 0,
		client_address,
		sizeof(sa_family_t) + strlen(client_address->sun_path) + 1);
	SB_LOG(SB_LOGLEVEL_DEBUG, "sendto => %d (%s)",
		(int)sent_msg_size, client_address->sun_path);
}

int receive_command_from_server_socket(struct sockaddr_un *client_address,
	ruletree_rpc_msg_command_t *command)
{
	ssize_t	received_msg_size;
	socklen_t addrlen = sizeof(struct sockaddr_un);
	fd_set in_set;
	int selected;

	FD_ZERO(&in_set);
	FD_SET(server_socket, &in_set);
	FD_SET(inotify_fd, &in_set);

	selected = select((server_socket < inotify_fd) ? (inotify_fd + 1) : (server_socket + 1),
		&in_set, NULL, NULL, NULL);
	if (selected <= 0) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: select returned %d?",
			__func__, selected);
		if (selected < 0) return(-2);
		return(RECEIVE_FAILED_TRY_AGAIN);
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "select => %d", selected);

	if (FD_ISSET(server_socket, &in_set)) {
		received_msg_size = recvfrom(server_socket, command, sizeof(*command), 0,
			(struct sockaddr*)client_address, &addrlen);
		SB_LOG(SB_LOGLEVEL_DEBUG, "recvfrom => %d (%s)", 
			(int)received_msg_size, client_address->sun_path);
		if (received_msg_size <= 0) {
			perror(progname);
			return (RECEIVE_FAILED_TRY_AGAIN);
		}
		/* FIXME: If message is too small... */
		return(RPC_COMMAND_RECEIVED);
	}
	if (FD_ISSET(inotify_fd, &in_set)) {
		char eventbuf[50 * (sizeof(struct inotify_event) + 30)];
		int eb_len, event_idx;

		SB_LOG(SB_LOGLEVEL_DEBUG, "I've been inotified");
		eb_len = read(inotify_fd, eventbuf, sizeof(eventbuf));
		if (eb_len < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Failed to receive inotify events",
				progname);
		} else for (event_idx = 0; event_idx < eb_len;) {
			struct inotify_event *ie = (struct inotify_event*)(eventbuf+event_idx);

			if (ie->wd != inotify_server_sock_dir_wd) {
				fprintf(stderr, "%s: Warning: received inotify "
					"event for an unknown descriptor\n", progname);
			} else {
				if (ie->mask & IN_DELETE) {
					/* normally this does not happen (see comment
					 * above) but if the delete events start
					 * to work someday... */
					SB_LOG(SB_LOGLEVEL_DEBUG, "%s: deleted = '%s'",
						__func__, ie->name);
					if (!strcmp(ie->name, "ssock")) {
						SB_LOG(SB_LOGLEVEL_DEBUG,
							"%s: server socket has been deleted.",
							__func__, ie->name);
						return(SOCKET_DELETED);
					}
				} else {
					SB_LOG(SB_LOGLEVEL_WARNING,
						"%s: Warning: received unexpected inotify "
						"event, mask=0x%X\n", progname, ie->mask);
				}
			}
			event_idx += sizeof(struct inotify_event) + ie->len;
		}
	}
	return(RECEIVE_FAILED_TRY_AGAIN);
}

