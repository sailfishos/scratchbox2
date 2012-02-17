/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* sb2d, the rule tree server:
 * The one and only process allowed to add data to the rule tree db
 * during an active SB2 session.
 *
 * Other processes may contact this server via a socket (AF_UNIX)
 * and ask the rule tree to be updated; reading from it is allowed
 * without locking, so care must be taken.
 *
 * This implementation assumes that writing 32-bit ints to the
 * memory-mapped file is an atomic operation.
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

#include "sb2_server.h"



void ruletree_server(void)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;
	struct sockaddr_un		client_address;

	SB_LOG(SB_LOGLEVEL_DEBUG, "Entering server loop");
	while (1) {
		int	r;

		SB_LOG(SB_LOGLEVEL_DEBUG, "get message");
		r = receive_command_from_server_socket(&client_address, &command);
		switch (r) {
		case RPC_COMMAND_RECEIVED:
			if (command.rimc_message_protocol_version !=
				RULETREE_RPC_PROTOCOL_VERSION) {
				SB_LOG(SB_LOGLEVEL_DEBUG, 
					"wrong protocol version %d",
						command.rimc_message_protocol_version);
			} else {
				SB_LOG(SB_LOGLEVEL_DEBUG, 
					"got command %d", command.rimc_message_type);
				switch (command.rimc_message_type) {
				case RULETREE_RPC_MESSAGE_COMMAND__PING:
					reply.rimr_message_type =
						RULETREE_RPC_MESSAGE_REPLY__OK;
					break;

				default:
					reply.rimr_message_type =
						RULETREE_RPC_MESSAGE_REPLY__UNKNOWNCMD;
				}
			}
			reply.rimr_message_protocol_version = command.rimc_message_protocol_version;
			reply.rimr_message_serial = command.rimc_message_serial;

			send_reply_to_client(&client_address, &reply);
			break;
		case RECEIVE_FAILED_TRY_AGAIN:
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"receive_command_from_server_socket failed, try again");
			break;
		case SOCKET_DELETED:
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"Socket has been deleted, exit.");
			return;
		default:
			SB_LOG(SB_LOGLEVEL_ERROR,
				"%s: Internal error: Unknown return code %d from "
				"receive_command_from_server_socket.", progname, r);
			return;
		}
	}
}
