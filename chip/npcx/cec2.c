/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "cec.h"
#include "clock_chip.h"
#include "console.h"
#include "ec_commands.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "mkbp_event.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#if !(DEBUG_CEC)
#define CPRINTF(...)
#define CPRINTS(...)
#else
#define CPRINTF(format, args...) cprintf(CC_CEC, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CEC, format, ## args)
#endif

/* Notification from interrupt to CEC task that data has been received */
#define TASK_EVENT_RECEIVED_DATA TASK_EVENT_CUSTOM_BIT(0)
#define TASK_EVENT_OKAY          TASK_EVENT_CUSTOM_BIT(1)
#define TASK_EVENT_FAILED        TASK_EVENT_CUSTOM_BIT(2)

/* Receive buffer and states */
struct cec_rx {
	/*
	 * The current incoming message being parsed. Copied to
	 * receive queue upon completion
	 */
	struct cec_msg_transfer transfer;
	/* End of Message received from source? */
	uint8_t eom;
	/* A follower NAK:ed a broadcast transfer */
	uint8_t broadcast_nak;
	/*
	 * Keep track of pulse low time to be able to verify
	 * pulse duration
	 */
	int low_ticks;
	/* Number of too short pulses seen in a row */
	int debounce_count;
};

/* Parameters and buffers for follower (receiver) state */
static struct cec_rx cec_rx;

/* Queue of completed incoming CEC messages */
static struct cec_rx_queue cec_rx_queue;

/* Events to send to AP */
static atomic_t cec_events;

static void send_mkbp_event(uint32_t event)
{
	atomic_or(&cec_events, event);
	mkbp_send_event(EC_MKBP_EVENT_CEC_EVENT);
}

void cec2_task(void *unused)
{
	int rv;
	uint32_t events;

	CPRINTF("CEC2 task starting\n");

	while (1) {
		events = task_wait_event(-1);
		if (events & TASK_EVENT_RECEIVED_DATA) {
			rv = cec_rx_queue_push(&cec_rx_queue,
					       cec_rx.transfer.buf,
					       cec_rx.transfer.byte);
			if (rv == EC_ERROR_OVERFLOW) {
				/* Queue full, prefer the most recent msg */
				cec_rx_queue_flush(&cec_rx_queue);
				rv = cec_rx_queue_push(&cec_rx_queue,
						       cec_rx.transfer.buf,
						       cec_rx.transfer.byte);
			}
			if (rv == EC_SUCCESS)
				mkbp_send_event(EC_MKBP_EVENT_CEC_MESSAGE);
		}
		if (events & TASK_EVENT_OKAY) {
			send_mkbp_event(EC_MKBP_CEC_SEND_OK);
			CPRINTS("SEND OKAY");
		} else if (events & TASK_EVENT_FAILED) {
			send_mkbp_event(EC_MKBP_CEC_SEND_FAILED);
			CPRINTS("SEND FAILED");
		}
	}
}
