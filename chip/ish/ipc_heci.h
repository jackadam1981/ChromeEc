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

enum {
	IPC_PEER_HOST_ID	= 0,
	IPC_PEER_PMC_ID		= 1,
	IPC_PEER_CSME_ID	= 2,
	IPC_PEERS_COUNT,
};

enum {
	IPC_PROTOCOL_BOOT = 0,
	IPC_PROTOCOL_HECI,
	IPC_PROTOCOL_MCTP,
	IPC_PROTOCOL_MNG,	/* Management protocol */
	IPC_PROTOCOL_ECP,	/* EC protocol */
	IPC_PROTOCOL_COUNT
};

#define IPC_MAX_PAYLOAD_SIZE		128
#define IPC_INVALID_HANDLE		0xFF

/*
 * Open ipc channel
 *
 * @param peer_id	select peer to communicate.
 * @param protocol	select protocol
 * @param event		set event flag
 *
 * @return		ipc handle or IPC_INVALID_HANDLE if there's error
 */
uint8_t ipc_open(uint8_t peer_id, uint8_t protocol, uint32_t event);
void ipc_close(uint8_t ipc_handle);

/*
 * Read message from ipc channel.
 * The function should be call by the same task called ipc_open().
 * The function waits until message is available.
 */
int ipc_read(uint8_t ipc_handle, void *buf, size_t buf_size);

/*
 * Read message from ipc channel.
 * The function should be call by the same task called ipc_open().
 * The function returns immediately if there's no message available.
 */
int ipc_read_nonblock(uint8_t ipc_handle, void *buf, size_t buf_size);

/* Write message to ipc channel. */
int ipc_write(uint8_t ipc_handle, const void *buf, size_t buf_size);

#endif /* __IPC_HECI_H */
