/* Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 * 
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#ifndef SB2_RULETREE_RPC_H__
#define SB2_RULETREE_RPC_H__

/* ------------ Rule tree RPC messages ------------
 * - for communicating with sb2d; any process running
 *   inside a session may make a remote procedure call
 *   to sb2d. Those calls are typically used to add/change
 *   information to the rule tree.
*/

#define RULETREE_RPC_PROTOCOL_VERSION	2

/* Commands: Client -> server messages */
typedef struct ruletree_rpc_msg_command_s {
	uint16_t	rimc_message_protocol_version;
	uint16_t	rimc_message_serial;
	uint32_t	rimc_message_type;
	union	{
		/* for most message types */
		uint32_t	rimm_status; 

		/* for SETFILEINFO */
		inodesimu_t	rimm_fileinfo;
	} rim_message;
} ruletree_rpc_msg_command_t;

#define RULETREE_RPC_MESSAGE_COMMAND__PING	1
#define RULETREE_RPC_MESSAGE_COMMAND__SETFILEINFO	2
#define RULETREE_RPC_MESSAGE_COMMAND__RELEASEFILEINFO	3
#define RULETREE_RPC_MESSAGE_COMMAND__CLEARFILEINFO	4

/* Replies: Server -> Client messages */
typedef struct ruletree_rpc_msg_reply_s {
	uint16_t	rimr_message_protocol_version; /* copied from command struct */
	uint16_t	rimr_message_serial; /* copied from command struct */
	uint32_t	rimr_message_type;
} ruletree_rpc_msg_reply_t;

#define RULETREE_RPC_MESSAGE_REPLY__OK		1	/* command ok. */
#define RULETREE_RPC_MESSAGE_REPLY__FAILED	2	/* command failed */
#define RULETREE_RPC_MESSAGE_REPLY__UNKNOWNCMD	3	/* unknown command */
#define RULETREE_RPC_MESSAGE_REPLY__PROTOVRSERR	4	/* wrong protocol version */

/* client-side RPC library: */
extern void ruletree_rpc__ping(void);

extern void ruletree_rpc__vperm_clear(uint64_t dev, uint64_t ino);

extern void ruletree_rpc__vperm_set_ids(uint64_t dev, uint64_t ino,
	int set_uid, uint32_t uid, int set_gid, uint32_t gid);
extern void ruletree_rpc__vperm_release_ids(uint64_t dev, uint64_t ino,
	int release_uid, int release_gid);
extern void ruletree_rpc__vperm_set_dev_node(uint64_t dev, uint64_t ino,
	mode_t mode, uint64_t rdev);

extern void ruletree_rpc__vperm_set_mode(uint64_t dev, uint64_t ino,
	mode_t real_mode, mode_t virt_mode, mode_t suid_sgid_bits);
extern void ruletree_rpc__vperm_release_mode(uint64_t dev, uint64_t ino);

#endif /* SB2_RULETREE_H__ */
