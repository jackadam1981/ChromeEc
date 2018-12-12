/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implement Nuvoton I2C over LPC protocol, as defined in:
 * https://goto.google.com/nuvoton-ec-i2c
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "i2c_over_lpc.h"
#include "lpc.h"
#include "motion_sense_hid.h"
#include "system.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)


uint8_t npcx_iol_msg[CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN];
enum npcx_iol_state npcx_iol_msg_state;
struct npcx_iol_msg *msg_from_host;

int npcx_iol_msg_idx;
int npcx_iol_msg_to_write_from_host;
int npcx_iol_msg_to_read_to_host;
int npcx_iol_msg_i2c_addr;


void npcx_iol_write_sem(uint8_t set, uint8_t unset)
{
	uint8_t old_sem = NPCX_IOL_SEM;

	/* Write semaphore and trigger an interrupt to the host. */
	npcx_iol_host_int(
		((old_sem & (~unset)) | set) & NPCX_IOL_CORE_SEM_MASK);
}

void npx_iol_shm_process_response(uint8_t status)
{
	uint8_t *dest;
	int len;

	CPRINTS("in npx_iol_shm_process_response");
	switch (npcx_iol_msg_state) {
	case IOL_CORE_SEND_BEGIN:
		msg_from_host->core_request.status = status;
		dest = msg_from_host->core_request.read_buf;
		len = MSG_FROM_HOST_SIZE - sizeof(msg_from_host->core_request);
		break;
	case IOL_CORE_SEND_CONT_ACK:
		dest = msg_from_host->core_request_cont.read_buf;
		len = MSG_FROM_HOST_SIZE;
		break;
	default:
		CPRINTS("unexpected state: %d", npcx_iol_msg_state);
	}
	len = MIN(len, npcx_iol_msg_to_read_to_host - npcx_iol_msg_idx);
	if (len > 0) {
		memcpy(dest, npcx_iol_msg + npcx_iol_msg_idx, len);
		CPRINTS("Write to buffer: %.*h", len, dest);
		npcx_iol_msg_idx += len;
	}
	if (npcx_iol_msg_idx < npcx_iol_msg_to_read_to_host)
		npcx_iol_msg_state = IOL_CORE_SEND_CONT;
	else
		npcx_iol_msg_state = IOL_CORE_SEND_END;
	npcx_iol_write_sem(NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, 0);
}

void npx_iol_send_response(int len)
{
	int status;

	if (len < 0) {
		npcx_iol_msg_to_read_to_host = 0;
		status = 0;
	} else {
		status = 1;
	}

	if (len < npcx_iol_msg_to_read_to_host)
		memset(npcx_iol_msg + len, 0,
		       npcx_iol_msg_to_read_to_host - len);

	/* Process I2C response */
	npcx_iol_msg_idx = 0;
	npcx_iol_msg_state = IOL_CORE_SEND_BEGIN;
	npx_iol_shm_process_response(status);
}

void npcx_iol_shm_process_fragment(uint8_t *fragment, int fragment_len)
{
	int len;

	/* Recopy the partial amount of data. */
	len = MIN(fragment_len, npcx_iol_msg_to_write_from_host -
			npcx_iol_msg_idx);
	memcpy(npcx_iol_msg + npcx_iol_msg_idx, fragment, len);
	CPRINTS("Process %d bytes.", len);
	npcx_iol_msg_idx += len;
	npcx_iol_msg_state = IOL_HOST_REQUEST_ACK;
	npcx_iol_write_sem(NPCX_IOL_CORE_CONT, 0);
}

void npcx_iol_task(void *u)
{
	while (1) {
		uint8_t sem;

		task_wait_event(-1);
		sem = NPCX_IOL_SEM;
		// CPRINTF("S %d, sem 0x%02x\n", npcx_iol_msg_state, sem);

		if (npcx_iol_msg_state == IOL_HOST_COMMAND_IN_PROGRESS) {
			/*
			 * A crosec host command is in progress.
			 * It has priority over I2C over LPC, so ignore that
			 * command.
			 */
			continue;
		}

		if ((sem & NPCX_IOL_HOST_REQUEST) &&
		    (npcx_iol_msg_state != IOL_IDLE)) {
			/*
			 * Reset state machine.
			 */
			CPRINTS("Aborted current IO");
			npcx_iol_msg_state = IOL_IDLE;
		}
		switch (npcx_iol_msg_state) {
		case IOL_IDLE:
			if (sem & NPCX_IOL_HOST_REQUEST) {
				npcx_iol_msg_idx = 0;
				/* Recopy content */
				npcx_iol_msg_i2c_addr =
					msg_from_host->host_request.address;
				npcx_iol_msg_to_write_from_host =
					msg_from_host->host_request.writes_nb;
				npcx_iol_msg_to_read_to_host =
					msg_from_host->host_request.reads_nb;
				CPRINTS("New message write %d - read %d",
					npcx_iol_msg_to_write_from_host,
					npcx_iol_msg_to_read_to_host);
				if ((npcx_iol_msg_to_read_to_host >
				     CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN) ||
				    (npcx_iol_msg_to_read_to_host >
				     CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN)) {
					CPRINTS("MSG too big");
					break;
				}
				if (npcx_iol_msg_to_read_to_host == 0 &&
				    npcx_iol_msg_to_write_from_host == 0) {
					CPRINTS("MSG too small");
					break;
				}
				npcx_iol_shm_process_fragment(
					msg_from_host->host_request.write_buf,
					MSG_FROM_HOST_SIZE -
					sizeof(msg_from_host->host_request));
			}
			break;
		case IOL_HOST_REQUEST_ACK:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_write_sem(0, NPCX_IOL_CORE_CONT);
				npcx_iol_msg_state = IOL_HOST_REQUEST_CONT_ACK;
			}
			break;
		case IOL_HOST_REQUEST_CONT_ACK:
			if (sem & NPCX_IOL_HOST_ACK)
				break;
			if (npcx_iol_msg_idx <
			    npcx_iol_msg_to_write_from_host)
				npcx_iol_shm_process_fragment(
				   msg_from_host->host_request_cont.write_buf,
				   MSG_FROM_HOST_SIZE);
			if (npcx_iol_msg_idx ==
			    npcx_iol_msg_to_write_from_host) {
				CPRINTS("Whole i2c packet ready: %d: %.*h",
					npcx_iol_msg_idx,
					npcx_iol_msg_idx, npcx_iol_msg);
				i2c_hid_process(npcx_iol_msg_idx, npcx_iol_msg,
						npx_iol_send_response);
			}
			break;
		case IOL_CORE_SEND_CONT:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_write_sem(NPCX_IOL_CORE_SEND,
						   NPCX_IOL_CORE_CONT);
				npcx_iol_msg_state = IOL_CORE_SEND_CONT_ACK;
			}
			break;
		case IOL_CORE_SEND_CONT_ACK:
			if (!(sem & NPCX_IOL_HOST_ACK))
				npx_iol_shm_process_response(0);
			break;
		case IOL_CORE_SEND_END:
			if (sem & NPCX_IOL_HOST_ACK) {
				npcx_iol_write_sem(0, NPCX_IOL_CORE_CONT);
				npcx_iol_msg_state = IOL_CORE_SEND_END_ACK;
			}
			break;
		case IOL_CORE_SEND_END_ACK:
			if (!(sem & NPCX_IOL_HOST_ACK)) {
				npcx_iol_write_sem(0, NPCX_IOL_CORE_SEND);
				npcx_iol_msg_state = IOL_CORE_SEND_END_ACK_ACK;
			}
			break;
		case IOL_CORE_SEND_END_ACK_ACK:
			/* Host reset ACK 2 times. */
			if (!(sem & NPCX_IOL_HOST_ACK))
				npcx_iol_msg_state = IOL_IDLE;
			break;

		case IOL_CORE_SEND_BEGIN:
			/* Impossible state, transient. */
		default:
			CPRINTS("unexpected state: %d", npcx_iol_msg_state);
		}
	}
}

