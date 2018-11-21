/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chip/mt_scp/registers.h"
#include "common.h"

#define IPC_MAX 1
#define IPC_ID(n) (n)

typedef void (*ipi_handler_t)(int id, void *data, uint32_t len);
typedef void (*ipc_handler_t)(void);

/*
 * Length of EC version string is at most 32 byte (NULL included), which
 * also aligns SCP fw_version length.
 */
#define SCP_FW_VERSION_LEN 32

/* IPI ID should share/sync across kernel and EC. */
enum ipi_id {
	IPI_SCP_INIT = 0,
	IPI_HOST_COMMAND,
	IPI_MDP,
	IPI_MAX
};

struct ipc_desc_t {
	ipc_handler_t handler;
};

struct ipi_desc_t {
	/* TODO(b/117917141): Port IPI stamp support.  */
	uint32_t is_wakeup_src;
	ipi_handler_t handler;
};

/*
 * Share buffer layout for IPI_SCP_INIT response. This structure should sync
 * across kernel and EC.
 */
struct scp_run_t {
	uint32_t signaled;
	int8_t fw_ver[SCP_FW_VERSION_LEN];
	uint32_t dec_capability;
	uint32_t enc_capability;
};

/*
 * The layout of the share buffer.
 * This should sync across kernel and EC.
 */
struct ipc_share_obj {
	/* IPC ID */
	int32_t id;
	/* Length of the contents in share_buf. */
	uint32_t len;
	/* Share buffer contents. */
	uint8_t share_buf[CONFIG_IPC_SHARE_BUF_SIZE];
};

typedef void (*ipi_handler_t)(int id, void *data, unsigned int len);

/*
 * Register a IPI handler.
 */
int ipi_register(enum ipi_id id, ipi_handler_t handler);

/*
 * Unregister a IPI handler.
 */
int ipi_unregister(enum ipi_id id);

/*
 * Send a IPI contents to AP.
 * Note: One should not call this function in ISR.
 */
int ipi_send(enum ipi_id id, void *buf, uint32_t len, int wait);

/*
 * Wake up AP via SPM Interrupt.
 */
void ipi_scp2spm(void);

/*
 * Register a IPC handler.
 */
void request_ipc(uint32_t ipc_id, ipc_handler_t handler);

/*
 * IPC Handler.
 */
void ipc_handler(void);
