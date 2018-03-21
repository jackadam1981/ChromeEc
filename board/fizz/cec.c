/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "cec.h"
#include "registers.h"
#include "gpio.h"
#include "console.h"
#include "host_command.h"
#include "util.h"
#include "task.h"
#include "ec_commands.h"
#include "fan_chip.h"

#if !(DEBUG_CEC)
#define CPUTS(...)
#define CPRINTS(...)
#else
#define CPUTS(outstr) cputs(CC_CEC, outstr)
#define CPRINTS(format, args...) cprints(CC_CEC, format, ## args)
#endif
/* Counter settings */
#define TCKC_CNT1_OFF 0xF8
#define TCKC_CNT1_SLOW 0x04
#define TCKC_CNT2_OFF 0xC7
#define TCKC_CNT2_SLOW 0x20
#define TCKC_CNT2_APB 0x08

/* Bit timing */
#define APB_FREQ 3750000
#define TO_CLOCK_TICK(MILLIS) (MILLIS * APB_FREQ / 10000)
#define CEC_FREE_TIME TO_CLOCK_TICK(24)

#define CEC_FREE_TIME_RS (3 * CEC_FREE_TIME) /* Resend */
#define CEC_FREE_TIME_NI (5 * CEC_FREE_TIME) /* New initiator */
#define CEC_FREE_TIME_PI (7 * CEC_FREE_TIME) /* Present initiator */

#define CEC_START_BIT_L TO_CLOCK_TICK(37)
#define CEC_START_BIT_H TO_CLOCK_TICK(8)

#define CEC_BIT_0_L TO_CLOCK_TICK(15)
#define CEC_BIT_0_H TO_CLOCK_TICK(9)

#define CEC_BIT_1_L TO_CLOCK_TICK(6)
#define CEC_BIT_1_H TO_CLOCK_TICK(18)

#define MAX_DATA_LEN 100 /* FIXME: What should this be?  */

enum cec_state {
	CEC_STATE_IDLE,
	CEC_STATE_START,
	CEC_STATE_DATA,
	CEC_STATE_EOM,
	CEC_STATE_ACK,
	CEC_STATE_ACK_VERIFY,
};

struct cec_tx {
	enum cec_state state;
	enum cec_state next_state;
	int out;
	int bits_sent;
	int bytes_sent;
	int data_len;
	char data[MAX_DATA_LEN];
};

static void tx_state_handler(void);

static struct cec_tx cec_tx;
static int cec_status = EC_RES_SUCCESS;

static void cec_tx_timer(void)
{
	/* Clear interrupt request. */
	SET_BIT(NPCX_TECLR(0), 3);

	tx_state_handler();
}

DECLARE_IRQ(NPCX_IRQ_MFT_1, cec_tx_timer, 1);

static void tx_state_handler(void)
{
	bool eom, data;
	uint16_t timeout = 0;
	enum cec_state prev_state;

	prev_state = cec_tx.state;
	cec_tx.state = cec_tx.next_state;

	switch (cec_tx.state) {
	case CEC_STATE_IDLE:
		/*
		 * Entering idle state, either command is
		 * completed or aborted
		 */
		NPCX_TCKC(0) &= TCKC_CNT2_OFF;
		cec_tx.out = 1;
		break;
	case CEC_STATE_START:
		if (cec_tx.out == 1) {
			/* Low to begin start-sequence */
			cec_tx.out = 0;
			timeout = CEC_START_BIT_L;
		} else {
			/* High to end start-sequence */
			cec_tx.out = 1;
			timeout = CEC_START_BIT_H;
			cec_tx.next_state = CEC_STATE_DATA;
		}
		break;
	case CEC_STATE_DATA:
		if (cec_tx.out == 1) {
			if (prev_state == CEC_STATE_DATA)
				cec_tx.bits_sent++;
			data = cec_tx.data[cec_tx.bytes_sent] &
				(1 << (7 - cec_tx.bits_sent));
			/* Low to begin data bit transfer */
			timeout = data ? CEC_BIT_1_L : CEC_BIT_0_L;
			cec_tx.out = 0;
		} else {
			data = cec_tx.data[cec_tx.bytes_sent] &
				(1 << (7 - cec_tx.bits_sent));
			/* High to end data bit transfer */
			timeout = data ? CEC_BIT_1_H : CEC_BIT_0_H;
			cec_tx.out = 1;
			if (cec_tx.bits_sent == 7) {
				/*
				 * When this bit is completed, after the
				 * timeout, will have sent 8 bits
				 */
				cec_tx.next_state = CEC_STATE_EOM;
			}
		}
		break;
	case CEC_STATE_EOM:
		if (cec_tx.out == 1) {
			cec_tx.bytes_sent++;
			eom = (cec_tx.bytes_sent == cec_tx.data_len);
			cec_tx.bits_sent = 0;
			/* Low to begin data EOM transfer */
			timeout =  eom ? CEC_BIT_1_L : CEC_BIT_0_L;
			cec_tx.out = 0;
		} else {
			/* High to end EOM transfer */
			eom = (cec_tx.bytes_sent == cec_tx.data_len);
			timeout = eom ? CEC_BIT_1_H : CEC_BIT_0_H;
			cec_tx.out = 1;
			cec_tx.next_state = CEC_STATE_ACK;
		}
		break;
	case CEC_STATE_ACK:
		if (cec_tx.out == 1) {
			/*
			 * Low to begin data ACK transfer, but only for a
			 * short while, then the sink should hold it down to ack
			 */
			timeout = CEC_BIT_1_L;
			cec_tx.out = 0;
		} else {
			/* Leave it to the sink draw the line low. */
			cec_tx.out = 1;
			timeout = CEC_BIT_1_H/4;
			cec_tx.next_state = CEC_STATE_ACK_VERIFY;
		}
		break;
	case CEC_STATE_ACK_VERIFY:
		/* Sample the signal to see if there is an ack or not */
		if (gpio_get_level(CEC_GPIO_IN)) {
			/* No ACK */
			cec_status = EC_RES_TIMEOUT;
			cec_tx.next_state = CEC_STATE_IDLE;

		} else {
			/* ACK received */
			if (cec_tx.bytes_sent == cec_tx.data_len) {
				cec_status = EC_RES_SUCCESS;
				cec_tx.next_state = CEC_STATE_IDLE;
			} else {
				cec_tx.next_state = CEC_STATE_DATA;
			}
		}
		cec_tx.out = 1;
		timeout = 3*CEC_BIT_1_H/4;
		break;
	};

	gpio_set_level(CEC_GPIO_OUT, cec_tx.out);
	if (timeout)
		NPCX_TCNT2(0) = timeout;
}

static int cec_send(const uint8_t *data, uint8_t len)
{
	int i;

	if (cec_status != 0)
		return EC_ERROR_BUSY;

	cec_status = EC_RES_BUSY;
	cec_tx.out = 1;
	cec_tx.state = CEC_STATE_IDLE;
	cec_tx.next_state = CEC_STATE_START;
	cec_tx.data_len = len;
	cec_tx.bits_sent = 0;
	cec_tx.bytes_sent = 0;

	CPRINTS("Send CEC:");
	for (i = 0; i < len && i < MAX_DATA_LEN; i++) {
		cec_tx.data[i] = data[i];
		CPRINTS(" 0x%02x", data[i]);
	}

	NPCX_TCNT2(0) = CEC_FREE_TIME_NI;
	NPCX_TCKC(0) |= TCKC_CNT2_APB;

	return EC_SUCCESS;
}

static int hc_cec(struct host_cmd_handler_args *args)
{
	const struct ec_params_cec_msg *params = args->params;

	if (params->msg_len == 0 || params->msg_len > MAX_CEC_MSG_LEN)
		return EC_ERROR_INVAL;

	return cec_send(params->msg, params->msg_len);
}

DECLARE_HOST_COMMAND(EC_CMD_CEC_WRITE_MSG, hc_cec, EC_VER_MASK(0));

static void tmr_init(void)
{
	int mdl = NPCX_MFT_MODULE_1;

	/* Ensure Multi-Function timer is powered up. */
	CLEAR_BIT(NPCX_PWDWN_CTL(mdl), NPCX_PWDWN_CTL1_MFT1_PD);

	/* Mode 2 - Dual-input capture */
	SET_FIELD(NPCX_TMCTRL(mdl), NPCX_TMCTRL_MDSEL_FIELD, NPCX_MFT_MDSEL_2);

	/* Enable capture on TA */
	SET_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TAEN);

	SET_BIT(NPCX_TCKC(mdl), NPCX_TCKC_LOW_PWR);

	SET_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TAIEN);
	SET_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TDIEN);


	/* Reset clock counters. */
	NPCX_TCNT1(mdl) = 0;
	NPCX_TCNT2(mdl) = 0;
}


int cec_init(void)
{
	/* Enable multifunction timer interrupt */
	task_enable_irq(NPCX_IRQ_MFT_1);

	/* Initialize timers */
	tmr_init();

	gpio_set_level(CEC_GPIO_PULL_UP, 1);

	CPRINTS("CEC enabled.");

	return 0;
}
