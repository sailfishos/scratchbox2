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
#if 0 && defined(not_yet)
#include "rule_tree_ipc.h"
#endif
#include "mapping.h"

#if 0 && defined(not_yet)
extern void initialize_server_address(void);
extern void create_server_socket(void);
extern void send_reply_to_client(struct sockaddr_un *client_address,
	ruletree_ipc_msg_reply_t *reply);
extern int receive_command_from_server_socket(struct sockaddr_un *client_address,
	ruletree_ipc_msg_command_t *command);
#endif

extern const char *progname;
extern char    *pid_file;

#endif /* SB2_SERVER_H__ */
