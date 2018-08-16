/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implement Nuvotion i2c over LPC protocol, as defined in:
 * https://drive.google.com/file/d/0B0DO3Pn_jl5cc2xvbkZaVkpjTDNURmwwZV9XU1BxaVVTdDFB/view
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "i2c_over_lpc.h"
#include "lpc.h"
#include "system.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)


uint8_t npcx_iol_msg[CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN];
enum npcx_iol_state npcx_iol_msg_state;
struct npcx_iol_msg *msg_from_host;

int npcx_iol_msg_idx;
int npcx_iol_msg_to_read_from_host;
int npcx_iol_msg_to_write_to_host;
int npcx_iol_msg_i2c_addr;


void npcx_iol_write_sem(uint8_t set, uint8_t unset)
{
	uint8_t old_sem = msg_from_host->semaphore;

	msg_from_host->semaphore = (old_sem & (~unset)) | set;
#ifdef TEST_BUILD
	/*
	 * Trigger an interrupt for in the test code to continue the data
	 * transfer.
	 */
	npcx_iol_host_int();
#endif
}

void npx_iol_shm_process_response(uint8_t status)
{
	uint8_t *dest;
	int len;

	switch (npcx_iol_msg_state) {
	case IOL_CORE_SEND_BEGIN:
		msg_from_host->core_request.status = status;
		dest = msg_from_host->core_request.read_buf;
		len = MSG_FROM_HOST_SIZE - sizeof(msg_from_host->core_request);
		break;
	case IOL_CORE_SEND_CONT:
		dest = msg_from_host->core_request_cont.read_buf;
		len = MSG_FROM_HOST_SIZE;
		break;
	default:
		CPRINTS("unexpected state: %d", npcx_iol_msg_state);
	}
	len = MIN(len, npcx_iol_msg_to_write_to_host - npcx_iol_msg_idx);
	memcpy(dest, npcx_iol_msg + npcx_iol_msg_idx, len);
	npcx_iol_msg_idx += len;
	if (npcx_iol_msg_idx < npcx_iol_msg_to_write_to_host)
		npcx_iol_msg_state = IOL_CORE_SEND_CONT;
	else
		npcx_iol_msg_state = IOL_CORE_SEND_END;
	npcx_iol_write_sem(NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, 0);
}

void npx_iol_send_response(int len)
{
	int status = (len != 0);

	npcx_iol_msg_to_write_to_host = MIN(npcx_iol_msg_to_write_to_host, len);

	/* Process I2C response */
	npcx_iol_msg_idx = 0;
	if (npcx_iol_msg_to_write_to_host > 0) {
		npcx_iol_msg_state = IOL_CORE_SEND_BEGIN;
		npx_iol_shm_process_response(status);
	} else {
		npcx_iol_msg_state = IOL_IDLE;
	}
}

void npcx_iol_shm_process_fragment(uint8_t *fragment, int fragment_len)
{
	int len;

	/* Recopy the partial amount of data. */
	len = MIN(fragment_len, npcx_iol_msg_to_read_from_host -
			npcx_iol_msg_idx);
	memcpy(npcx_iol_msg + npcx_iol_msg_idx, fragment, len);
	npcx_iol_msg_idx += len;
	if (npcx_iol_msg_idx < npcx_iol_msg_to_read_from_host) {
		npcx_iol_msg_state = IOL_HOST_REQUEST_CONT_ACK;
		npcx_iol_write_sem(NPCX_IOL_CORE_CONT, 0);
		return;
	}
	i2c_hid_process(npcx_iol_msg_i2c_addr, npcx_iol_msg_idx,
			npcx_iol_msg, npx_iol_send_response);
}

void npcx_iol_task(void *u)
{
	uint32_t event = 0;

	while (1) {
		uint8_t sem = msg_from_host->semaphore;

		CPRINTS("In state %d, sem 0x%02x, event 0x%08x",
			npcx_iol_msg_state, sem, event);
		if (sem & NPCX_IOL_HOST_REQUEST) {
			/*
			 * Reset state machine.
			 */
			if (npcx_iol_msg_state != IOL_IDLE)
				CPRINTS("Aborted current IO");
			npcx_iol_msg_state = IOL_IDLE;
		}
		switch (npcx_iol_msg_state) {
		case IOL_IDLE:
			if (sem & NPCX_IOL_HOST_REQUEST) {
				npcx_iol_msg_idx = 0;
				npcx_iol_msg_state = IOL_HOST_REQUEST_ACK;
				npcx_iol_write_sem(NPCX_IOL_CORE_CONT, 0);
			}
			break;
		case IOL_HOST_REQUEST_ACK:
		case IOL_HOST_REQUEST_CONT_ACK:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_msg_state++;
				npcx_iol_write_sem(0, NPCX_IOL_CORE_CONT);
			}
			break;
		case IOL_HOST_REQUEST_RCV:
			if (!(sem & ~NPCX_IOL_HOST_ACK)) {
				/* Recopy content */
				npcx_iol_msg_to_read_from_host =
					msg_from_host->host_request.writes_nb;
				npcx_iol_msg_to_write_to_host =
					msg_from_host->host_request.reads_nb;
				if ((npcx_iol_msg_to_write_to_host >
				     CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN) ||
				    (npcx_iol_msg_to_write_to_host >
				     CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN)) {
					CPRINTS("MSG too big");
					break;
				}
				npcx_iol_shm_process_fragment(
					msg_from_host->host_request.write_buf,
					MSG_FROM_HOST_SIZE -
					sizeof(msg_from_host->host_request));
			}
			break;
		case IOL_HOST_REQUEST_CONT_RCV:
			if (!(sem & ~NPCX_IOL_HOST_ACK)) {
				npcx_iol_shm_process_fragment(
					msg_from_host->host_request_cont.write_buf,
					MSG_FROM_HOST_SIZE);
			}
			break;
		case IOL_CORE_SEND_BEGIN:
			/* Impossible state, transient */
			break;

		case IOL_CORE_SEND_CONT:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_write_sem(NPCX_IOL_CORE_SEND,
						NPCX_IOL_CORE_CONT);
				npx_iol_shm_process_response(0);
			}
			break;
		case IOL_CORE_SEND_END:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_write_sem(0,
					NPCX_IOL_CORE_CONT |
					NPCX_IOL_CORE_SEND);
				npcx_iol_msg_state = IOL_IDLE;
			}
			break;
		default:
			CPRINTS("unexpected state: %d", npcx_iol_msg_state);
		}
		event = task_wait_event(-1);
	}
}

