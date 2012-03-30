/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#ifndef SB2_SERVER_H__
#define SB2_SERVER_H__

#include "sb2.h"
#include "rule_tree.h"
#include "rule_tree_rpc.h"
#include "mapping.h"

extern char *execute_init2_script(void);

extern void create_server_socket(void);
extern void ruletree_server(void);

extern void send_reply_to_client(struct sockaddr_un *client_address,
	ruletree_rpc_msg_reply_t *reply,
	size_t reply_size);

extern int receive_command_from_server_socket(struct sockaddr_un *client_address,
	ruletree_rpc_msg_command_t *command);
/* return codes from receive_command_from_server_socket(): */
#define RPC_COMMAND_RECEIVED		1
#define RECEIVE_FAILED_TRY_AGAIN	2
#define	SOCKET_DELETED			3

extern const char *progname;
extern char    *pid_file;

#endif /* SB2_SERVER_H__ */
