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

enum ipc_peer_id {
	IPC_PEER_ID_HOST	= 0, /* x64 host */
	IPC_PEER_ID_PMC		= 1, /* Power Management Controller */
	IPC_PEER_ID_CSME	= 2, /* Converged Security Management Engine */
	IPC_PEERS_COUNT,
};
BUILD_ASSERT(IPC_PEERS_COUNT <= 0x0F);

enum ipc_protocol {
	IPC_PROTOCOL_BOOT = 0,	/* Not supported */
	IPC_PROTOCOL_HECI,	/* Host Embedded Controller Interface */
	IPC_PROTOCOL_MCTP,	/* not supported */
	IPC_PROTOCOL_MNG,	/* Management protocol */
	IPC_PROTOCOL_ECP,	/* EC Protocol. not supported */
	IPC_PROTOCOL_COUNT
};
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
ipc_handle_t ipc_open(const enum ipc_peer_id peer_id,
		      const enum ipc_protocol protocol,
		      const uint32_t event);
void ipc_close(const ipc_handle_t handle);

/*
 * Read message from ipc channel.
 * The function should be call by the same task called ipc_open().
 * The function waits until message is available.
 * @param timeout_us	if == -1, wait until message is available.
			if == 0, return immediately.
			if > 0, wait for the specified microsecond duration time
 */
int ipc_read(const ipc_handle_t handle, void *buf, const size_t buf_size,
             int timeout_us);

/* Write message to ipc channel. */
int ipc_write(const ipc_handle_t handle, const void *buf,
	      const size_t buf_size);

#endif /* __IPC_HECI_H */
