/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IPC module for ISH */
#ifndef __IPC_HECI_H
#define __IPC_HECI_H

#define IPC_ERR_INVALID_HANDLE		-1
#define IPC_ERR_UNEXPECTED_EVENT	-2
#define IPC_ERR_IPC_IS_NOT_READY	-3
#define IPC_ERR_TOO_BIG_MSG		-4
#define IPC_ERR_TOO_SMALL_BUFFER	-5
#define IPC_ERR_QUEUE_FULL		-6
#define IPC_ERR_INVALID_TASK		-7
#define IPC_ERR_MSG_NOT_AVAILABLE	-8
#define IPC_ERR_INVALID_MSG		-9

typedef enum {
	IPC_PEER_ID_HOST	= 0,
	IPC_PEER_ID_PMC		= 1,
	IPC_PEER_ID_CSME	= 2,
	IPC_PEERS_COUNT,
} ipc_peer_id_t;
BUILD_ASSERT(IPC_PEERS_COUNT <= 0x0F);

typedef enum {
	IPC_PROTOCOL_BOOT = 0,
	IPC_PROTOCOL_HECI,
	IPC_PROTOCOL_MCTP,
	IPC_PROTOCOL_MNG,	/* Management protocol */
	IPC_PROTOCOL_ECP,	/* EC protocol */
	IPC_PROTOCOL_COUNT
} ipc_protocol_t;
BUILD_ASSERT(IPC_PROTOCOL_COUNT <= 0x0F);

typedef void *				ipc_handle_t;

#define IPC_MAX_PAYLOAD_SIZE		128
#define IPC_INVALID_HANDLE		NULL

/*
 * Open ipc channel
 *
 * @param peer_id	select peer to communicate.
 * @param protocol	select protocol
 * @param event		set event flag
 *
 * @return		ipc handle or IPC_INVALID_HANDLE if there's error
 */
ipc_handle_t ipc_open(ipc_peer_id_t peer_id, ipc_protocol_t protocol,
		      uint32_t event);
void ipc_close(ipc_handle_t handle);

/*
 * Read message from ipc channel.
 * The function should be call by the same task called ipc_open().
 * The function waits until message is available.
 */
int ipc_read(ipc_handle_t handle, void *buf, size_t buf_size);

/*
 * Read message from ipc channel.
 * The function should be call by the same task called ipc_open().
 * The function returns immediately if there's no message available.
 */
int ipc_read_nonblock(ipc_handle_t handle, void *buf, size_t buf_size);

/* Write message to ipc channel. */
int ipc_write(ipc_handle_t handle, const void *buf, size_t buf_size);

#endif /* __IPC_HECI_H */
