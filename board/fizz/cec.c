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

#if !(DEBUG_CEC)
#define CPUTS(...)
#define CPRINTS(...)
#else
#define CPUTS(outstr) cputs(CC_GPIO, outstr)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)
#endif

/* Multi-function timer settings */
#define TMR_INT_EN 0x09
#define TMCTRL_INIT 0x21
#define TCKC_INIT 0x80
#define MAX_BYTE 5

/* Counter settings */
#define TCKC_CNT1_OFF 0xF8 //ANDed
#define TCKC_CNT1_SLOW 0x04 //ORed
#define TCKC_CNT2_OFF 0xC7 //ANDed
#define TCKC_CNT2_SLOW 0x20 //ORed
#define TCKC_CNT2_APB 0x08 //ORed

/* Bit timing */
#define APB_FREQ 3750000
#define TO_CLOCK_TICK(MILLIS) (MILLIS * APB_FREQ / 10000)
#define CEC_FREE_TIME TO_CLOCK_TICK(24)

#define CEC_FREE_TIME_RS (3 * CEC_FREE_TIME) // Resend
#define CEC_FREE_TIME_NI (5 * CEC_FREE_TIME) // New initiator
#define CEC_FREE_TIME_PI (7 * CEC_FREE_TIME) // Present initiator

#define CEC_START_BIT_L TO_CLOCK_TICK(37)
#define CEC_START_BIT_H TO_CLOCK_TICK(8)

#define CEC_BIT_0_L TO_CLOCK_TICK(15)
#define CEC_BIT_0_H TO_CLOCK_TICK(9)

#define CEC_BIT_1_L TO_CLOCK_TICK(6)
#define CEC_BIT_1_H TO_CLOCK_TICK(18)

#define MAX_DATA_LEN 100 // FIXME: What should this be?

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
		// Entering idle state, either command is completed or aborted
		NPCX_TCKC(0) &= TCKC_CNT2_OFF;
		cec_tx.out = 1;
		break;
	case CEC_STATE_START:
		if (cec_tx.out == 1) {
			// Low to begin start-sequence
			cec_tx.out = 0;
			timeout = CEC_START_BIT_L;
		} else {
			// High to end start-sequence
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
			// Low to begin data bit transfer
			timeout = data ? CEC_BIT_1_L : CEC_BIT_0_L;
			cec_tx.out = 0;
		} else {
			data = cec_tx.data[cec_tx.bytes_sent] &
				(1 << (7 - cec_tx.bits_sent));
			// High to end data bit transfer
			timeout = data ? CEC_BIT_1_H : CEC_BIT_0_H;
			cec_tx.out = 1;
			if (cec_tx.bits_sent == 7) {
				// When this bit is completed, after the
				// timeout, will have sent 8 bits
				cec_tx.next_state = CEC_STATE_EOM;
			}
		}
		break;
	case CEC_STATE_EOM:
		if (cec_tx.out == 1) {
			cec_tx.bytes_sent++;
			eom = (cec_tx.bytes_sent == cec_tx.data_len);
			cec_tx.bits_sent = 0;
			// Low to begin data EOM transfer
			timeout =  eom ? CEC_BIT_1_L : CEC_BIT_0_L;
			cec_tx.out = 0;
		} else {
			// High to end EOM transfer
			eom = (cec_tx.bytes_sent == cec_tx.data_len);
			timeout = eom ? CEC_BIT_1_H : CEC_BIT_0_H;
			cec_tx.out = 1;
			cec_tx.next_state = CEC_STATE_ACK;
		}
		break;
	case CEC_STATE_ACK:
		if (cec_tx.out == 1) {
			// Low to begin data ACK transfer, but only for a
			// short while, then the sink should hold it down to ack
			timeout = CEC_BIT_1_L;
			cec_tx.out = 0;
		} else {
			// Leave it to the sink draw the line low.
			cec_tx.out = 1;
			timeout = CEC_BIT_1_H/4;
			cec_tx.next_state = CEC_STATE_ACK_VERIFY;
		}
		break;
	case CEC_STATE_ACK_VERIFY:
		// Sample the signal to see if there is an ack or not
		if (gpio_get_level(CEC_GPIO_IN)) {
			// No ACK
			cec_status = EC_RES_TIMEOUT;
			cec_tx.next_state = CEC_STATE_IDLE;

		} else {
			// ACK received
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

int cec_send(uint8_t data[], int8_t byte_len)
{
	int i;

	cec_status = EC_RES_BUSY;
	cec_tx.out = 1;
	cec_tx.state = CEC_STATE_IDLE;
	cec_tx.next_state = CEC_STATE_START;
	cec_tx.data_len = byte_len;
	cec_tx.bits_sent = 0;
	cec_tx.bytes_sent = 0;
	for (i = 0; i < byte_len && i < MAX_DATA_LEN; i++)
		cec_tx.data[i] = data[i];

	NPCX_TCNT2(0) = CEC_FREE_TIME_NI;
	NPCX_TCKC(0) |= TCKC_CNT2_APB;

	return 0;
}


static int tv_on(void)
{
	uint8_t data[2];

	CPRINTS("Turning TV on\n");

	data[0] = 0x40;
	data[1] = 0x04;
	return cec_send(data, 2);
}

static int tv_off(void)
{
	uint8_t data[2];

	CPRINTS("Turning TV off\n");

	data[0] = 0x40;
	data[1] = 0x36;
	return cec_send(data, 2);
}

static int command_cec(int argc, char **argv)
{
	if (argc != 2)
		return EC_ERROR_UNKNOWN;

	if (!strncmp(argv[1], "tv_on", 5)) {
		return tv_on();
	} else if (!strncmp(argv[1], "tv_off", 6)) {
		return tv_off();
	} else if (!strncmp(argv[1], "status", 6)) {
		CPRINTS("CEC status: %d\n", cec_status);
		return cec_status;
	}

	return EC_ERROR_UNIMPLEMENTED;
}

DECLARE_SAFE_CONSOLE_COMMAND(cec, command_cec, NULL, NULL);

static int hc_display_power(struct host_cmd_handler_args *args)
{
	const struct ec_params_cec *params = args->params;

	switch (params->cec_cmd) {
	case CEC_CMD_TV_ON:
		return tv_on();
	case CEC_CMD_TV_OFF:
		return tv_off();
	case CEC_CMD_STATUS:
		return cec_status;
	};

	return EC_ERROR_UNIMPLEMENTED;
}

DECLARE_HOST_COMMAND(EC_CMD_DISPLAY_POWER, hc_display_power, EC_VER_MASK(0));

int cec_init(void)
{
	/* Ensure Multi-Function timer is powered up. */
	SET_FIELD(NPCX_PWDWN_CTL(0), FIELD(5, 1), 0);

	/* Enable multifunction timer interrupt */
	task_enable_irq(NPCX_IRQ_MFT_1);

	/* Set timer controls */
	NPCX_TMCTRL(0) = TMCTRL_INIT;
	NPCX_TCKC(0) = TCKC_INIT;
	NPCX_TIEN(0) = TMR_INT_EN;

	/* Reset clock counters. */
	NPCX_TCNT1(0) = 0;
	NPCX_TCNT2(0) = 0;

	gpio_set_level(CEC_GPIO_PULL_UP, 1);

	CPRINTS("CEC enabled.");

	return 0;
}
