/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_IPI_CHIP_H
#define __CROS_EC_IPI_CHIP_H

#include "chip/mt_scp/registers.h"
#include "common.h"

#define IPC_MAX 1
#define IPC_ID(n) (n)

typedef void (*ipi_handler_t)(int id, void *data, uint32_t len);
typedef void (*ipi_wakeup_t)(void);
typedef void (*ipc_handler_t)(void);

/*
 * Length of EC version string is at most 32 byte (NULL included), which
 * also aligns SCP fw_version length.
 */
#define SCP_FW_VERSION_LEN 32

#ifndef IPI_SCP_INIT
#error Must define IPI_SCP_INIT if enable IPI.
#endif

struct ipc_desc_t {
	ipc_handler_t handler;
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
 * The layout of the IPC0 AP/SCP shared buffer.
 * This should sync across kernel and EC.
 */
struct ipc_shared_obj {
	/* IPI ID */
	int32_t id;
	/* Length of the contents in shared_buf. */
	uint32_t len;
	/* Shared buffer contents. */
	uint8_t shared_buf[CONFIG_IPC_SHARED_OBJ_SHARED_BUF_SIZE];
};

typedef void (*ipi_handler_t)(int id, void *data, unsigned int len);

/* Send a IPI contents to AP. */
int ipi_send(int32_t id, void *buf, uint32_t len, int wait);

/*
 * IPC Handler.
 */
void ipc_handler(void);

/* IPI tables */
extern ipi_handler_t ipi_handler_table[];
extern ipi_wakeup_t ipi_wakeup_table[];

/* Helper macros to build the IPI handler and wakeup functions. */
#define IPI_HANDLER(id) CONCAT3(ipi_, id, _handler)
#define IPI_WAKEUP(id) CONCAT3(ipi_, id, _wakeup)

/*
 * Macro to connect the IPI handler "handler" to the IPI number "_id" and store
 * function pointer in the ipi_handler_table.
 */
#define DECLARE_IPI(_id, handler)                                              \
	typedef struct {                                                       \
		int dummy[_id < IPI_COUNT ? 1 : -1];                           \
	} ipi_num_check1_##ipi;                                                \
	void __keep IPI_HANDLER(_id)(int32_t id, void *buf, uint32_t len)      \
	{                                                                      \
		handler(id, buf, len);                                         \
	}

/*
 * Macro to declare an IPI is a AP wake-up source and store the function
 * pointer in the ipi_wakeup_table.  Wake up AP via SPM interrupt.
 */
#define DECLARE_IPI_WAKEUP_SRC(_id)                                            \
	typedef struct {                                                       \
		int dummy[_id < IPI_COUNT ? 1 : -1];                           \
	} ipi_num_check2_##ipi;                                                \
	void __keep IPI_WAKEUP(_id)(void) { SCP_SPM_INT = SPM_INT_A2SPM; }

#endif /* __CROS_EC_IPI_CHIP_H */
