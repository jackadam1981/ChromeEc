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

/* Time in us to timer clock ticks */
#define APB1_TICKS(t) ((t) * apb1_freq_div_10k / 100)
#if DEBUG_CEC
/* Timer clock ticks to us */
#define APB1_US(ticks) (100*(ticks)/apb1_freq_div_10k)
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

/* APB1 frequency. Store divided by 10k to avoid some runtime divisions */
static uint32_t apb1_freq_div_10k;

static void send_mkbp_event(uint32_t event)
{
	atomic_or(&cec_events, event);
	mkbp_send_event(EC_MKBP_EVENT_CEC2_EVENT);
}

static int cec2_get_next_event(uint8_t *out)
{
	uint32_t event_out = atomic_clear(&cec_events);

	memcpy(out, &event_out, sizeof(event_out));

	return sizeof(event_out);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_CEC2_EVENT, cec2_get_next_event);

static int cec2_get_next_msg(uint8_t *out)
{
	int rv;
	uint8_t msg_len, msg[MAX_CEC_MSG_LEN];

	rv = cec_rx_queue_pop(&cec_rx_queue, msg, &msg_len);
	if (rv != 0)
		return EC_RES_UNAVAILABLE;

	memcpy(out, msg, msg_len);

	return msg_len;
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_CEC2_MESSAGE, cec2_get_next_msg);

static void cec2_init(void)
{
	int mdl = NPCX_MFT_MODULE_2;

	/* APB1 is the clock we base the timers on */
	apb1_freq_div_10k = clock_get_apb1_freq()/10000;

	/* Ensure Multi-Function timer is powered up. */
	CLEAR_BIT(NPCX_PWDWN_CTL(mdl), NPCX_PWDWN_CTL1_MFT2_PD);

	/* Mode 2 - Dual-input capture */
	SET_FIELD(NPCX_TMCTRL(mdl), NPCX_TMCTRL_MDSEL_FIELD, NPCX_MFT_MDSEL_2);

	/* Enable capture TCNT1 into TCRA and preset TCNT1. */
	SET_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TBEN);

	/* If RO doesn't set it, RW needs to set it explicitly. */
	gpio_set_level(CEC2_GPIO_PULL_UP, 1);

	/* Ensure the CEC bus is not pulled low by default on startup. */
	gpio_set_level(CEC2_GPIO_OUT, 1);

	CPRINTS("CEC2 initialized");
}
DECLARE_HOOK(HOOK_INIT, cec2_init, HOOK_PRIO_LAST);

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
				mkbp_send_event(EC_MKBP_EVENT_CEC2_MESSAGE);
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
