/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* Rule tree client-side library:
 * To be used from inside an active SB2 session for
 * connecting to sb2d, the rule tree db server process.
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

#include "mapping.h"
#include "sb2.h"
#include "libsb2.h"
#include "rule_tree.h"
#include "rule_tree_rpc.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <sys/socket.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

#include "exported.h"

static struct sockaddr_un server_address;
static socklen_t	server_addr_len;
static struct sockaddr_un client_address;
static char *client_socket_path = NULL;

static int client_socket = -1;
static int server_address_initialized = 0;

static int initialize_server_address(void)
{
	char	*sock_path = NULL;
	size_t	sock_path_len;

	if (server_address_initialized) return(0);

	if (!sbox_session_dir) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: session directory is missing.");
		return(-1);
	}
	if (asprintf(&sock_path, "%s/sb2d-sock.d/ssock", sbox_session_dir) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: asprintf failed");
		return(-1);
	}
	sock_path_len = strlen(sock_path);
	if (sock_path_len >= sizeof(server_address.sun_path)-1) {
		/* This should never happen, server should not start
		 * if address would be too long
		 * (and session creation should fail), but check anyways */
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: server socket address is too long (%s)",
			sock_path);
		return(-1);
	}
	server_address.sun_family = AF_UNIX;
	strcpy(server_address.sun_path, sock_path); /* it fits. */
	server_addr_len = sizeof(sa_family_t) + sock_path_len + 1;
	free(sock_path);

	server_address_initialized = 1;
	return(0);
}

static void cleanup_client_socket(void)
{
	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_rpc: cleanup");
	if (client_socket >= 0) {
		close(client_socket);
		client_socket = -1;
	}
	if (client_socket_path) {
		unlink_nomap_nolog(client_socket_path);
		free(client_socket_path);
		client_socket_path = NULL;
	}
}

static int create_client_socket(void)
{
	socklen_t	client_addr_len;
	size_t		sock_path_len;
	int		min_fd;

	client_socket = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (client_socket < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: Failed to create client socket");
		goto error_out;
	}
	min_fd = ruletree_get_min_client_socket_fd();
	if (client_socket < min_fd) {
		/* find lowest free fd above min_fd */
		int new_fd = fcntl(client_socket, F_DUPFD, (long)min_fd);
		if (new_fd < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"ruletree_rpc: failed to move socket FD > %d",
				min_fd);
		} else {
			close(client_socket);
			client_socket = new_fd;
			SB_LOG(SB_LOGLEVEL_NOISE,
				"ruletree_rpc: client socket FD = %d",
				client_socket);
		}
	}
	if (asprintf(&client_socket_path, "%s/sock/%d", sbox_session_dir, getpid()) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: asprintf failed");
		goto error_out;
	}
	sock_path_len = strlen(client_socket_path);
	if (sock_path_len >= sizeof(client_address.sun_path)-1) {
		/* This should never happen, server should not start
		 * if address would be too long
		 * (and session creation should fail), but check anyways */
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: client socket address is too long (%s)",
			client_socket_path);
		goto error_out;
	}
	client_address.sun_family = AF_UNIX;
	strcpy(client_address.sun_path, client_socket_path); /* it fits. */
	unlink_nomap_nolog(client_socket_path); /* remove old socket, if any. */

	client_addr_len = sizeof(sa_family_t) + sock_path_len + 1;
	if (bind_nomap_nolog(client_socket, (struct sockaddr*)&client_address, client_addr_len) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"ruletree_rpc: Failed to bind client socket address (%s)",
			client_socket_path);
		goto error_out;
	}

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"ruletree_rpc: client socket = (%s)",
		client_socket_path);
	/* client socket has been initialized. */
	atexit(cleanup_client_socket);
	return(0);

   error_out:
	cleanup_client_socket();
	return(-1);
}

/* use a mutex to allow only one thread to access the socket, if libpthreads is available.
 * If it isn't, this is used in a sigle-threaded program and we can
 * safely live without the mutex.
*/
static pthread_mutex_t	client_socket_mutex = PTHREAD_MUTEX_INITIALIZER;

static int send_command_receive_reply(
	ruletree_rpc_msg_command_t	*command,
	ruletree_rpc_msg_reply_t	*reply)
{
	ssize_t	sent_msg_size;
	ssize_t	received_msg_size;
	int use_locking = 0;

	if (pthread_library_is_available) {
		use_locking = 1;
		SB_LOG(SB_LOGLEVEL_NOISE, "Going to lock client_socket_mutex");
		(*pthread_mutex_lock_fnptr)(&client_socket_mutex);
	}

	if (server_address_initialized == 0) {
		if (initialize_server_address() < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Failed to initialize server socket address (ruletree_rpc)");
			goto error_out;
		}
	}

    reopen_socket:
	if (client_socket < 0) {
		if (create_client_socket() < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Failed to create client socket (ruletree_rpc)");
			goto error_out;
		}
	}

	command->rimc_message_protocol_version = RULETREE_RPC_PROTOCOL_VERSION;
	/* FIXME: fill serial */
	sent_msg_size = sendto_nomap_nolog(client_socket, command, sizeof(*command), 0,
		(struct sockaddr*)&server_address, server_addr_len);
	if (sent_msg_size < 0) {
		switch (errno) {
		case ENOTSOCK:
		case EBADF:
			/* bad fd; assume the application has closed or reused
			 * it; because it might be open for another use already,
			 * we don't attempt to close it here. */
			client_socket = -1;
			goto reopen_socket;
		}

		SB_LOG(SB_LOGLEVEL_ERROR,
			"Failed to send command to server (ruletree_rpc)");
		goto error_out;
	}

	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_rpc: sendto => %d", (int)sent_msg_size);
		
	received_msg_size = recvfrom_nomap_nolog(client_socket, reply, sizeof(*reply), 0,
		(struct sockaddr*)NULL, (socklen_t*)NULL);
	SB_LOG(SB_LOGLEVEL_DEBUG, "ruletree_rpc: recvfrom => %d", (int)received_msg_size);
	/* FIXME: check serial */
	/* FIXME: check sender address? */
	if (received_msg_size <= 0) {
		goto error_out;
	}
	/* FIXME: If message is too small... */
	if (use_locking) {
		(*pthread_mutex_unlock_fnptr)(&client_socket_mutex);
		SB_LOG(SB_LOGLEVEL_NOISE, "unlocked client_socket_mutex");
	}
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"%s: Received reply type=%u", __func__, reply->rimr_message_type);
	return(0);

    error_out:
	if (use_locking) {
		(*pthread_mutex_unlock_fnptr)(&client_socket_mutex);
		SB_LOG(SB_LOGLEVEL_NOISE, "unlocked client_socket_mutex");
	}
	return(-1);
}

void ruletree_rpc__ping(void)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"ruletree_rpc: Sending command 'ping'");
	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__PING;
	send_command_receive_reply(&command, &reply);
}

/* clear vperm info completely. */
void ruletree_rpc__vperm_clear(uint64_t dev, uint64_t ino)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__CLEARFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	send_command_receive_reply(&command, &reply);
}

void ruletree_rpc__vperm_set_ids(uint64_t dev, uint64_t ino,
	int set_uid, uint32_t uid, int set_gid, uint32_t gid)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	if (set_uid) 
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: uid=%d", __func__, uid);
	if (set_gid) 
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s: gid=%d", __func__, gid);
	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__SETFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	command.rim_message.rimm_fileinfo.inodesimu_active_fields =
		(set_uid ? RULETREE_INODESTAT_SIM_UID : 0) |
		(set_gid ? RULETREE_INODESTAT_SIM_GID : 0);
	command.rim_message.rimm_fileinfo.inodesimu_uid = uid;
	command.rim_message.rimm_fileinfo.inodesimu_gid = gid;
	send_command_receive_reply(&command, &reply);
}

void ruletree_rpc__vperm_release_ids(uint64_t dev, uint64_t ino,
	int release_uid, int release_gid)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s: %s %s", __func__,
		(release_uid?"rel.uid":""), (release_gid?"rel.gid":""));
	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__RELEASEFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	command.rim_message.rimm_fileinfo.inodesimu_active_fields =
		(release_uid ? RULETREE_INODESTAT_SIM_UID : 0) |
		(release_gid ? RULETREE_INODESTAT_SIM_GID : 0);
	send_command_receive_reply(&command, &reply);
}

void ruletree_rpc__vperm_set_mode(uint64_t dev, uint64_t ino,
	mode_t real_mode, mode_t virt_mode, mode_t suid_sgid_bits)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__SETFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	command.rim_message.rimm_fileinfo.inodesimu_mode = virt_mode;
	command.rim_message.rimm_fileinfo.inodesimu_suidsgid = suid_sgid_bits;

	if ((real_mode & ~(S_ISUID | S_ISGID)) != 
	    (virt_mode & ~(S_ISUID | S_ISGID))) {
		command.rim_message.rimm_fileinfo.inodesimu_active_fields |=
			RULETREE_INODESTAT_SIM_MODE;
	}

	if (suid_sgid_bits != (real_mode & (S_ISUID | S_ISGID))) {
		command.rim_message.rimm_fileinfo.inodesimu_active_fields |=
			RULETREE_INODESTAT_SIM_SUIDSGID;
	}
	send_command_receive_reply(&command, &reply);
}

void ruletree_rpc__vperm_release_mode(uint64_t dev, uint64_t ino)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__RELEASEFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	command.rim_message.rimm_fileinfo.inodesimu_active_fields =
		RULETREE_INODESTAT_SIM_MODE | RULETREE_INODESTAT_SIM_SUIDSGID;
	send_command_receive_reply(&command, &reply);
}

void ruletree_rpc__vperm_set_dev_node(uint64_t dev, uint64_t ino,
        mode_t mode, uint64_t rdev)
{
	ruletree_rpc_msg_command_t	command;
	ruletree_rpc_msg_reply_t	reply;

	memset(&command, 0, sizeof(command));
	command.rimc_message_type = RULETREE_RPC_MESSAGE_COMMAND__SETFILEINFO;
	command.rim_message.rimm_fileinfo.inodesimu_dev = dev;
	command.rim_message.rimm_fileinfo.inodesimu_ino = ino;
	command.rim_message.rimm_fileinfo.inodesimu_active_fields =
		RULETREE_INODESTAT_SIM_MODE | RULETREE_INODESTAT_SIM_DEVNODE;
	command.rim_message.rimm_fileinfo.inodesimu_mode = mode & (~S_IFMT);
	command.rim_message.rimm_fileinfo.inodesimu_devmode = mode & S_IFMT;
	command.rim_message.rimm_fileinfo.inodesimu_rdev = rdev;
	send_command_receive_reply(&command, &reply);
}


