/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NPCX-specific I2C over i2c protocol */

#ifndef __CROS_EC_I2C_OVER_LPC_H
#define __CROS_EC_I2C_OVER_LPC_H

#include "common.h"

#ifndef TEST_BUILD
#include "chip/npcx/lpc_chip.h"
#define NPCX_IOL_SEM         NPCX_SHAW_SEM(1)
#else
#define NPCX_IOL_SEM         msg_from_host->semaphore
#endif

#define __NPCX_IOL_HOST_REQUEST 0
#define __NPCX_IOL_HOST_ACK     1
#define __NPCX_IOL_CORE_SEND    4
#define __NPCX_IOL_CORE_CONT    5

#define NPCX_IOL_HOST_REQUEST (1 << __NPCX_IOL_HOST_REQUEST)
#define NPCX_IOL_HOST_ACK     (1 << __NPCX_IOL_HOST_ACK)
#define NPCX_IOL_CORE_SEND    (1 << __NPCX_IOL_CORE_SEND)
#define NPCX_IOL_CORE_CONT    (1 << __NPCX_IOL_CORE_CONT)

#define NPCX_IOL_HOST_SEM_MASK		0x0F
#define NPCX_IOL_CORE_SEM_MASK		0xF0


#define MSG_FROM_HOST_SIZE (EC_HOST_CMD_REGION_SIZE - sizeof(uint8_t))

/* Structure overlay on top of the host packet area */
struct __packed npcx_iol_msg {
#ifdef TEST_BUILD
	uint8_t semaphore;
#else
	/* NPCX route the semaphore to register SHAW1_SEM */
	uint8_t reserved;
#endif
	union {
		struct __packed {
			uint8_t address;
			uint16_t writes_nb;
			uint16_t reads_nb;
			uint8_t write_buf[0];
		} host_request;
		struct __packed {
			uint8_t write_buf[0];
		} host_request_cont;

		struct __packed {
			uint8_t status;
			uint8_t read_buf[0];
		} core_request;
		struct __packed {
			uint8_t read_buf[0];
		} core_request_cont;
	};
};

enum npcx_iol_state {
	IOL_IDLE,
	IOL_HOST_COMMAND_IN_PROGRESS,
	IOL_HOST_REQUEST_ACK,
	IOL_HOST_REQUEST_CONT_ACK,
	IOL_CORE_SEND_BEGIN,
	IOL_CORE_SEND_CONT,
	IOL_CORE_SEND_CONT_ACK,
	IOL_CORE_SEND_END,
	IOL_CORE_SEND_END_ACK,
	IOL_CORE_SEND_END_ACK_ACK,
};

extern enum npcx_iol_state npcx_iol_msg_state;
extern struct npcx_iol_msg *msg_from_host;

#define TASK_EVENT_IOL_PENDING     TASK_EVENT_CUSTOM(1)

int npcx_iol_host_int(int semaphore);


#endif  /*  __CROS_EC_I2C_OVER_LPC_H */

