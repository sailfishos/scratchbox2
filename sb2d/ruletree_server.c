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


static void ruletree_cmd_clearfileinfo(
	ruletree_rpc_msg_command_t *command,
	ruletree_rpc_msg_reply_t *reply)
{
        inodesimu_t			istat_in_db;
	ruletree_inodestat_handle_t	handle;

	SB_LOG(SB_LOGLEVEL_DEBUG, "clearfileinfo dev=%lld ino=%lld",
		(long long)command->rim_message.rimm_fileinfo.inodesimu_dev,
		(long long)command->rim_message.rimm_fileinfo.inodesimu_ino);

	ruletree_init_inodestat_handle(&handle,
		command->rim_message.rimm_fileinfo.inodesimu_dev,
		command->rim_message.rimm_fileinfo.inodesimu_ino);

	if (ruletree_find_inodestat(&handle, &istat_in_db) < 0) {
		/* not found. */
		SB_LOG(SB_LOGLEVEL_DEBUG, "clearfileinfo: not found");
	} else {
		/* found - update */

		SB_LOG(SB_LOGLEVEL_DEBUG, "clearfileinfo: found");
		if (istat_in_db.inodesimu_active_fields != 0) {
			istat_in_db.inodesimu_active_fields = 0;
			ruletree_set_inodestat(&handle, &istat_in_db);
			/* FIXME ###################### Check return value */
			dec_vperm_num_active_inodestats();
		}
		reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__OK;
	}
}

static void ruletree_cmd_setfileinfo(
	ruletree_rpc_msg_command_t *command,
	ruletree_rpc_msg_reply_t *reply)
{
        inodesimu_t			istat_in_db;
	ruletree_inodestat_handle_t	handle;

	istat_in_db = command->rim_message.rimm_fileinfo;
	SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo dev=%lld ino=%lld",
		(long long)istat_in_db.inodesimu_dev,
		(long long)istat_in_db.inodesimu_ino);

	ruletree_init_inodestat_handle(&handle,
		command->rim_message.rimm_fileinfo.inodesimu_dev,
		command->rim_message.rimm_fileinfo.inodesimu_ino);

	if (ruletree_find_inodestat(&handle, &istat_in_db) < 0) {
		/* not found. */
		SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo: not found, set");
		ruletree_set_inodestat(&handle, &command->rim_message.rimm_fileinfo);
		/* FIXME ###################### Check return value */
		inc_vperm_num_active_inodestats();
		reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__OK;
	} else {
		/* found - update */
		uint32_t prev_active_fields = istat_in_db.inodesimu_active_fields;

		SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo: found, update");
		if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
		    RULETREE_INODESTAT_SIM_UID) {
        		istat_in_db.inodesimu_uid = command->rim_message.rimm_fileinfo.inodesimu_uid;
        		istat_in_db.inodesimu_active_fields |= RULETREE_INODESTAT_SIM_UID;
			SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo: found, set uid to %d",
				istat_in_db.inodesimu_uid);
		}
		if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
		    RULETREE_INODESTAT_SIM_GID) {
        		istat_in_db.inodesimu_gid = command->rim_message.rimm_fileinfo.inodesimu_gid;
        		istat_in_db.inodesimu_active_fields |= RULETREE_INODESTAT_SIM_GID;
			SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo: found, set gid to %d",
				istat_in_db.inodesimu_gid);
		}
		if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
		    (RULETREE_INODESTAT_SIM_MODE | RULETREE_INODESTAT_SIM_SUIDSGID)) {
        		istat_in_db.inodesimu_mode = command->rim_message.rimm_fileinfo.inodesimu_mode;
        		istat_in_db.inodesimu_suidsgid = command->rim_message.rimm_fileinfo.inodesimu_suidsgid;
        		istat_in_db.inodesimu_active_fields &=
				~(RULETREE_INODESTAT_SIM_MODE | RULETREE_INODESTAT_SIM_SUIDSGID);
        		istat_in_db.inodesimu_active_fields |=
				command->rim_message.rimm_fileinfo.inodesimu_active_fields &
				(RULETREE_INODESTAT_SIM_MODE | RULETREE_INODESTAT_SIM_SUIDSGID);
        		istat_in_db.inodesimu_active_fields |= RULETREE_INODESTAT_SIM_MODE;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"setfileinfo: found, set mode to 0%o, suid/sgid=0%o",
				istat_in_db.inodesimu_mode,
				istat_in_db.inodesimu_suidsgid);
		}
		if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
		    RULETREE_INODESTAT_SIM_DEVNODE) {
        		istat_in_db.inodesimu_devmode = command->rim_message.rimm_fileinfo.inodesimu_devmode;
        		istat_in_db.inodesimu_rdev = command->rim_message.rimm_fileinfo.inodesimu_rdev;
        		istat_in_db.inodesimu_active_fields |= RULETREE_INODESTAT_SIM_DEVNODE;
			SB_LOG(SB_LOGLEVEL_DEBUG, "setfileinfo: found, set device: 0%o, 0x%X",
				istat_in_db.inodesimu_devmode,
				(int)istat_in_db.inodesimu_rdev);
		}
		ruletree_set_inodestat(&handle, &istat_in_db);
		/* FIXME ###################### Check return value */

		if ((prev_active_fields == 0) &&
		    (istat_in_db.inodesimu_active_fields != 0)) {
			/* was present, but inactive and has now
			 * been reactivated. */
			inc_vperm_num_active_inodestats();
		}
		reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__OK;
	}
}

static void ruletree_cmd_releasefileinfo(
	ruletree_rpc_msg_command_t *command,
	ruletree_rpc_msg_reply_t *reply)
{
        inodesimu_t			istat_in_db;
	ruletree_inodestat_handle_t	handle;

	istat_in_db = command->rim_message.rimm_fileinfo;
	SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo dev=%lld ino=%lld",
		(long long)istat_in_db.inodesimu_dev,
		(long long)istat_in_db.inodesimu_ino);

	ruletree_init_inodestat_handle(&handle,
		command->rim_message.rimm_fileinfo.inodesimu_dev,
		command->rim_message.rimm_fileinfo.inodesimu_ino);

	if (ruletree_find_inodestat(&handle, &istat_in_db) < 0) {
		/* not found. don't have to do anything. */
		SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: not found");
		reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__OK;
	} else {
		/* found - update */
		uint32_t prev_active_fields = istat_in_db.inodesimu_active_fields;

		if (command->rim_message.rimm_fileinfo.inodesimu_active_fields) {
			/* there is something active */
			SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: found, update");
			if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
			    RULETREE_INODESTAT_SIM_UID) {
				SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: release uid");
				istat_in_db.inodesimu_active_fields &= ~RULETREE_INODESTAT_SIM_UID;
			}
			if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
			    RULETREE_INODESTAT_SIM_GID) {
				SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: release gid");
				istat_in_db.inodesimu_active_fields &= ~RULETREE_INODESTAT_SIM_GID;
			}
			if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
			    RULETREE_INODESTAT_SIM_MODE) {
				SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: found, release mode");
				istat_in_db.inodesimu_active_fields &= ~RULETREE_INODESTAT_SIM_MODE;
			}
			if (command->rim_message.rimm_fileinfo.inodesimu_active_fields &
			    RULETREE_INODESTAT_SIM_DEVNODE) {
				SB_LOG(SB_LOGLEVEL_DEBUG, "releasefileinfo: found, release device node");
				istat_in_db.inodesimu_active_fields &= ~RULETREE_INODESTAT_SIM_DEVNODE;
			}
		}
		ruletree_set_inodestat(&handle, &istat_in_db);
		/* FIXME ###################### Check return value */
		if ((prev_active_fields != 0) &&
		    (istat_in_db.inodesimu_active_fields == 0)) {
			/* went to inactive state. */
			dec_vperm_num_active_inodestats();
		}
		reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__OK;
	}
}

static void ruletree_cmd_init2(ruletree_rpc_msg_reply_t *reply)
{
	char *result;

	result = execute_init2_script();
	strcpy(reply->msg.rimr_str, (result ? result : "No result"));
	if (result) free(result);
	reply->hdr.rimr_message_type = RULETREE_RPC_MESSAGE_REPLY__MESSAGE;
}

void ruletree_server(void)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;
	struct sockaddr_un		client_address;

	SB_LOG(SB_LOGLEVEL_DEBUG, "Entering server loop");
	while (1) {
		int	r;
		size_t	reply_size = sizeof(ruletree_rpc_msg_reply_hdr_t);

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
					reply.hdr.rimr_message_type =
						RULETREE_RPC_MESSAGE_REPLY__OK;
					break;

				case RULETREE_RPC_MESSAGE_COMMAND__INIT2:
					ruletree_cmd_init2(&reply);
					reply_size = sizeof(ruletree_rpc_msg_reply_hdr_t) +
						strlen(reply.msg.rimr_str) + 1;
					break;

				case RULETREE_RPC_MESSAGE_COMMAND__SETFILEINFO:
					ruletree_cmd_setfileinfo(&command,&reply);
					break;

				case RULETREE_RPC_MESSAGE_COMMAND__RELEASEFILEINFO:
					ruletree_cmd_releasefileinfo(&command,&reply);
					break;

				case RULETREE_RPC_MESSAGE_COMMAND__CLEARFILEINFO:
					ruletree_cmd_clearfileinfo(&command,&reply);
					break;

				default:
					reply.hdr.rimr_message_type =
						RULETREE_RPC_MESSAGE_REPLY__UNKNOWNCMD;
				}
			}
			reply.hdr.rimr_message_protocol_version = command.rimc_message_protocol_version;
			reply.hdr.rimr_message_serial = command.rimc_message_serial;

			send_reply_to_client(&client_address, &reply, reply_size);
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
