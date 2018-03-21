/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "clock_chip.h"
#include "console.h"
#include "ec_commands.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_CEC, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CEC, format, ## args)

/* Time in us to clock ticks of apb1 timer */
/* TODO(sadolfsson): This doesn't look nice, round off or precalc so we don't need uint64_t */
#define APB1_TICKS(t_us) ((uint64_t)(t_us) * (uint64_t)apb1_freq / 1000000)
#define APB1_US(ticks) (((uint64_t)1000000)*((uint64_t)(ticks))/((uint64_t)apb1_freq))

#define FREE_TIME APB1_TICKS(2400)
#define FREE_TIME_RS (3 * (FREE_TIME)) /* Resend */
#define FREE_TIME_NI (5 * (FREE_TIME)) /* New initiator */
#define FREE_TIME_PI (7 * (FREE_TIME)) /* Present initiator */

/* Start bit timing */
#define START_BIT_LOW_TIME APB1_TICKS(3700)
#define START_BIT_MIN_LOW_TIME APB1_TICKS(3500)
#define START_BIT_MAX_LOW_TIME APB1_TICKS(3900)
#define START_BIT_HIGH_TIME APB1_TICKS(800)
#define START_BIT_MIN_HIGH_TIME APB1_TICKS(600)
#define START_BIT_MAX_HIGH_TIME APB1_TICKS(1000)

/* Data bit timing */
#define LOGICAL_ZERO_LOW_TIME APB1_TICKS(1500)
#define LOGICAL_ZERO_MIN_LOW_TIME APB1_TICKS(1300)
#define LOGICAL_ZERO_MAX_LOW_TIME APB1_TICKS(1700)
#define LOGICAL_ZERO_HIGH_TIME APB1_TICKS(900)
#define LOGICAL_ZERO_MIN_HIGH_TIME APB1_TICKS(750)
#define LOGICAL_ZERO_MAX_HIGH_TIME APB1_TICKS(1050)

#define LOGICAL_ONE_LOW_TIME APB1_TICKS(600)
#define LOGICAL_ONE_MIN_LOW_TIME APB1_TICKS(400)
#define LOGICAL_ONE_MAX_LOW_TIME APB1_TICKS(800)
#define LOGICAL_ONE_HIGH_TIME APB1_TICKS(1800)
#define LOGICAL_ONE_MIN_HIGH_TIME APB1_TICKS(1650)
#define LOGICAL_ONE_MAX_HIGH_TIME APB1_TICKS(1950)

#define DATA_BIT_MAX_LOW_TIME (MAX(LOGICAL_ZERO_MAX_LOW_TIME, LOGICAL_ONE_MAX_LOW_TIME))
#define DATA_BIT_MAX_HIGH_TIME (MAX(LOGICAL_ZERO_MAX_HIGH_TIME, LOGICAL_ONE_MAX_HIGH_TIME))

#define DATA_TIME(type, data) ((data) ? (LOGICAL_ONE_ ## type ## _TIME) : \
					(LOGICAL_ZERO_ ## type ## _TIME))
#define DATA_HIGH_TIME(data) DATA_TIME(HIGH, data)
#define DATA_LOW_TIME(data) DATA_TIME(LOW, data)

#define VALID_TIME(type, bit, t) ((t) >= bit ## _MIN_ ## type ## _TIME && \
					(t) <=  bit ##_MAX_ ## type ## _TIME)
#define VALID_LOW_TIME(bit, t) VALID_TIME(LOW, bit, t)
#define VALID_HIGH_TIME(bit, t) VALID_TIME(HIGH, bit, t)
#define VALID_DURATION_TIME(bit, t) VALID_TIME(DURATION, bit, t)

enum cec_state {
	CEC_STATE_IDLE = 0,
	CEC_STATE_INITIATOR_FREE_TIME,
	CEC_STATE_INITIATOR_START_LOW,
	CEC_STATE_INITIATOR_START_HIGH,
	CEC_STATE_INITIATOR_HEADER_INIT_LOW,
	CEC_STATE_INITIATOR_HEADER_INIT_HIGH,
	CEC_STATE_INITIATOR_HEADER_DEST_LOW,
	CEC_STATE_INITIATOR_HEADER_DEST_HIGH,
	CEC_STATE_INITIATOR_DATA_LOW,
	CEC_STATE_INITIATOR_DATA_HIGH,
	CEC_STATE_INITIATOR_EOM_LOW,
	CEC_STATE_INITIATOR_EOM_HIGH,
	CEC_STATE_INITIATOR_ACK_LOW,
	CEC_STATE_INITIATOR_ACK_HIGH,
	CEC_STATE_INITIATOR_ACK_VERIFY,
	CEC_STATE_FOLLOWER_START_LOW,
	CEC_STATE_FOLLOWER_START_HIGH,
	CEC_STATE_FOLLOWER_HEADER_INIT_LOW,
	CEC_STATE_FOLLOWER_HEADER_INIT_HIGH,
	CEC_STATE_FOLLOWER_HEADER_DEST_LOW,
	CEC_STATE_FOLLOWER_HEADER_DEST_HIGH,
	CEC_STATE_FOLLOWER_EOM_LOW,
	CEC_STATE_FOLLOWER_EOM_HIGH,
	CEC_STATE_FOLLOWER_ACK_LOW,
	CEC_STATE_FOLLOWER_ACK_FINISH,
	CEC_STATE_FOLLOWER_DATA_LOW,
	CEC_STATE_FOLLOWER_DATA_HIGH,
};

struct cec_transfer {
	int bit;
	int byte;
	uint8_t data[MAX_CEC_MSG_LEN];
};

struct cec_transfer_state {
	enum cec_state state;
	struct cec_transfer rx;
	struct cec_transfer tx;
	int tx_len;
	int resends;
	bool ack;
	bool eom;
	int cap_start;
};

enum cap_edge {
	CAP_EDGE_FALLING,
	CAP_EDGE_RISING
};

static struct cec_transfer_state cec_ts;

/* TODO(sadolfsson): save the high low-time variables instead */
static int apb1_freq;

#if DEBUG_CEC
/* Debugging TODO(sadolfsson): Remove or cleanup */
#define STATE_LOG_SIZE 30
#define EVENT_LOG_SIZE 30

struct cec_state_log {
	enum cec_state state;
	int gpio;
	int tmr_tmo;
	int cap_tmo;
	int cap_edge;
};

struct cec_ev_log {
	enum cec_state state;
	int event;
	uint16_t tcra;
	uint16_t tcnt1;
	uint16_t tien;
	int cap_start;
};

static struct cec_state_log state_log[STATE_LOG_SIZE];
static struct cec_ev_log event_log[EVENT_LOG_SIZE];
unsigned state_log_idx;
unsigned event_log_idx;
#endif

static void cap_start(enum cap_edge edge, uint16_t tmo)
{
	int mdl = NPCX_MFT_MODULE_1;

	/* Select edge to trigger capture on */
	UPDATE_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TAEDG, edge == CAP_EDGE_RISING);

	/*
	 * Set capture timeout. If we don't have a timeout, we
	 * turn the timeout interrupt off and only care about
	 * the edge change.
	 */
	if (tmo) {
		NPCX_TCNT1(mdl) = tmo;
		SET_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TCIEN);
	} else {
		CLEAR_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TCIEN);
		NPCX_TCNT1(mdl) = 0;
	}

	/* Clear out old events */
	SET_BIT(NPCX_TECLR(mdl), NPCX_TECLR_TACLR);
	SET_BIT(NPCX_TECLR(mdl), NPCX_TECLR_TCCLR);
	NPCX_TCRA(mdl) = 0;
	/* Start the capture timer */
	SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C1CSEL_FIELD, 1);
}

static void cap_stop(void)
{
	int mdl = NPCX_MFT_MODULE_1;
	SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C1CSEL_FIELD, 0);
}

static int cap_get_time(int start_time)
{
	int mdl = NPCX_MFT_MODULE_1;
	return (start_time - (int)NPCX_TCRA(mdl));
}

static void tmr_start(uint16_t tmo)
{
	int mdl = NPCX_MFT_MODULE_1;
	NPCX_TCNT2(mdl) = tmo;
	SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C2CSEL_FIELD, 1);
}

static void tmr_stop(void)
{
	int mdl = NPCX_MFT_MODULE_1;
	SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C2CSEL_FIELD, 0);
}

static void tr_inc_bit_offset(struct cec_transfer *tr)
{
	if (++(tr->bit) == 8) {
		tr->bit = 0;
		tr->byte++;
	}
}

static void tr_set_data_bit(struct cec_transfer *tr, bool val)
{
	uint8_t bit;

	bit = 1 << (7 - tr->bit);
	tr->data[tr->byte] &= ~bit;
	if (val)
		tr->data[tr->byte] |= bit;
}

static bool tr_get_data_bit(struct cec_transfer *tr)
{
	return tr->data[tr->byte] & (1 << (7 - tr->bit));
}


static bool is_eom(void)
{
	if (cec_ts.tx.bit != 0)
		return false;

	return (cec_ts.tx.byte == cec_ts.tx_len);
}


void enter_state(enum cec_state new_state)
{
	int gpio = -1, cap_tmo = -1, tmr_tmo = -1;
	enum cap_edge cap_edge = -1;

	cec_ts.state = new_state;
	switch (new_state) {
	case CEC_STATE_IDLE:
		if (cec_ts.tx_len > 0) {
			/* Execute a postponed send */
			enter_state(CEC_STATE_INITIATOR_FREE_TIME);
		} else {
			/* Wait for incoming command */
			gpio = 1;
			cap_edge = CAP_EDGE_FALLING;
		}
		break;
	case CEC_STATE_INITIATOR_FREE_TIME:
		gpio = 1;
		cap_edge = CAP_EDGE_FALLING;
		if (cec_ts.resends)
			cap_tmo = FREE_TIME_RS;
		else
			cap_tmo = FREE_TIME_NI;
		break;
	case CEC_STATE_INITIATOR_START_LOW:
		cec_ts.tx.bit = 0;
		cec_ts.tx.byte = 0;
		gpio = 0;
		tmr_tmo = START_BIT_LOW_TIME;
		break;
	case CEC_STATE_INITIATOR_START_HIGH:
		gpio = 1;
		cap_edge = CAP_EDGE_FALLING;
		cap_tmo = START_BIT_HIGH_TIME;
		break;
	case CEC_STATE_INITIATOR_HEADER_INIT_LOW:
	case CEC_STATE_INITIATOR_HEADER_DEST_LOW:
	case CEC_STATE_INITIATOR_DATA_LOW:
		gpio = 0;
		tmr_tmo = DATA_LOW_TIME(tr_get_data_bit(&cec_ts.tx));
		break;
	case CEC_STATE_INITIATOR_HEADER_INIT_HIGH:
		gpio = 1;
		cap_edge = CAP_EDGE_FALLING;
		cap_tmo = DATA_HIGH_TIME(tr_get_data_bit(&cec_ts.tx));
		break;
	case CEC_STATE_INITIATOR_HEADER_DEST_HIGH:
	case CEC_STATE_INITIATOR_DATA_HIGH:
		gpio = 1;
		tmr_tmo = DATA_HIGH_TIME(tr_get_data_bit(&cec_ts.tx));
		break;
	case CEC_STATE_INITIATOR_EOM_LOW:
		gpio = 0;
		tmr_tmo = DATA_LOW_TIME(is_eom());
		break;
	case CEC_STATE_INITIATOR_EOM_HIGH:
		gpio = 1;
		tmr_tmo = DATA_HIGH_TIME(is_eom());
		break;
	case CEC_STATE_INITIATOR_ACK_LOW:
		gpio = 0;
		tmr_tmo = DATA_LOW_TIME(1);
		break;
	case CEC_STATE_INITIATOR_ACK_HIGH:
		gpio = 1;
		/* Aim for the middle of the safe sample time */
		tmr_tmo = (LOGICAL_ONE_LOW_TIME + LOGICAL_ZERO_LOW_TIME)/2 - LOGICAL_ONE_LOW_TIME;
		break;
	case CEC_STATE_INITIATOR_ACK_VERIFY:
		cec_ts.ack = !gpio_get_level(CEC_GPIO_OUT);
		/*
		 * The ACK is nominally 2.4 ms, and the middle of the safe sample time is at (0.85+1.25)/2 ms.
		 * We are at the safe sample time right now.
		 */
		tmr_tmo = APB1_TICKS(2400 - (850 + 1250)/2);
		break;
	case CEC_STATE_FOLLOWER_START_LOW:
		cap_edge = CAP_EDGE_RISING;
		cap_tmo = START_BIT_MAX_LOW_TIME;
		break;
	case CEC_STATE_FOLLOWER_START_HIGH:
		cap_edge = CAP_EDGE_FALLING;
		cap_tmo = START_BIT_MAX_HIGH_TIME;
		break;
	case CEC_STATE_FOLLOWER_HEADER_INIT_LOW:
	case CEC_STATE_FOLLOWER_HEADER_DEST_LOW:
	case CEC_STATE_FOLLOWER_EOM_LOW:
		cap_edge = CAP_EDGE_RISING;
		cap_tmo = DATA_BIT_MAX_LOW_TIME;
		break;
	case CEC_STATE_FOLLOWER_HEADER_INIT_HIGH:
	case CEC_STATE_FOLLOWER_HEADER_DEST_HIGH:
	case CEC_STATE_FOLLOWER_EOM_HIGH:
		cap_edge = CAP_EDGE_FALLING;
		cap_tmo = DATA_BIT_MAX_HIGH_TIME;
		break;
	case CEC_STATE_FOLLOWER_ACK_LOW:
		/* TODO(sadolfsson): Check for broadcast or my-addr */
		gpio = 0;
		tmr_tmo = LOGICAL_ZERO_LOW_TIME;
		break;
	case CEC_STATE_FOLLOWER_ACK_FINISH:
		gpio = 1;
		if (cec_ts.eom) {
			tmr_tmo = LOGICAL_ZERO_HIGH_TIME;
		} else {
			cap_edge = CAP_EDGE_FALLING;
			cap_tmo = DATA_BIT_MAX_HIGH_TIME;
		}
		break;
	case CEC_STATE_FOLLOWER_DATA_LOW:
		cap_edge = CAP_EDGE_RISING;
		cap_tmo = DATA_BIT_MAX_LOW_TIME;
		break;
	case CEC_STATE_FOLLOWER_DATA_HIGH:
		cap_edge = CAP_EDGE_FALLING;
		cap_tmo = DATA_BIT_MAX_HIGH_TIME;
		break;
	}
#if DEBUG_CEC
	if (state_log_idx < STATE_LOG_SIZE) {
		state_log[state_log_idx].state = cec_ts.state;
		state_log[state_log_idx].cap_tmo = cap_tmo;
		state_log[state_log_idx].tmr_tmo = tmr_tmo;
		state_log[state_log_idx].cap_edge = cap_edge;
		state_log_idx++;
	}
#endif
	if (gpio >= 0)
		gpio_set_level(CEC_GPIO_OUT, gpio);
	if (cap_tmo >= 0) {
		cec_ts.cap_start = cap_tmo;
		cap_start(cap_edge, cap_tmo);
	}
	if (tmr_tmo > 0)
		tmr_start(tmr_tmo);
}

static void cec_ev_tmo(void)
{
	switch (cec_ts.state) {
	case CEC_STATE_IDLE:
		break;
	case CEC_STATE_INITIATOR_FREE_TIME:
		enter_state(CEC_STATE_INITIATOR_START_LOW);
		break;
	case CEC_STATE_INITIATOR_START_LOW:
		enter_state(CEC_STATE_INITIATOR_START_HIGH);
		break;
	case CEC_STATE_INITIATOR_START_HIGH:
		enter_state(CEC_STATE_INITIATOR_HEADER_INIT_LOW);
		break;
	case CEC_STATE_INITIATOR_HEADER_INIT_LOW:
		enter_state(CEC_STATE_INITIATOR_HEADER_INIT_HIGH);
		break;
	case CEC_STATE_INITIATOR_HEADER_INIT_HIGH:
		tr_inc_bit_offset(&cec_ts.tx);
		if (cec_ts.tx.bit == 4)
			enter_state(CEC_STATE_INITIATOR_HEADER_DEST_LOW);
		else
			enter_state(CEC_STATE_INITIATOR_HEADER_INIT_LOW);
		break;
	case CEC_STATE_INITIATOR_HEADER_DEST_LOW:
		enter_state(CEC_STATE_INITIATOR_HEADER_DEST_HIGH);
		break;
	case CEC_STATE_INITIATOR_HEADER_DEST_HIGH:
		tr_inc_bit_offset(&cec_ts.tx);
		if (cec_ts.tx.byte == 1)
			enter_state(CEC_STATE_INITIATOR_EOM_LOW);
		else
			enter_state(CEC_STATE_INITIATOR_HEADER_DEST_LOW);
		break;
	case CEC_STATE_INITIATOR_EOM_LOW:
		enter_state(CEC_STATE_INITIATOR_EOM_HIGH);
		break;
	case CEC_STATE_INITIATOR_EOM_HIGH:
		enter_state(CEC_STATE_INITIATOR_ACK_LOW);
		break;
	case CEC_STATE_INITIATOR_ACK_LOW:
		enter_state(CEC_STATE_INITIATOR_ACK_HIGH);
		break;
	case CEC_STATE_INITIATOR_ACK_HIGH:
		enter_state(CEC_STATE_INITIATOR_ACK_VERIFY);
		break;
	case CEC_STATE_INITIATOR_ACK_VERIFY:
		if (cec_ts.ack) {
			if (!is_eom()) {
				/* More data in this frame */
				enter_state(CEC_STATE_INITIATOR_DATA_LOW);
			} else {
				/* Transfer completed successfully */
				cec_ts.tx_len = 0;
				cec_ts.resends = 0;
				enter_state(CEC_STATE_IDLE);
			}
		} else {
			if (cec_ts.resends < 5) {
				/* Resend */
				cec_ts.resends++;
				enter_state(CEC_STATE_INITIATOR_FREE_TIME);
			} else {
				/* Transfer failed */
				cec_ts.tx_len = 0;
				cec_ts.resends = 0;
				enter_state(CEC_STATE_IDLE);
			}
		}
		break;
	case CEC_STATE_INITIATOR_DATA_LOW:
		enter_state(CEC_STATE_INITIATOR_DATA_HIGH);
		break;
	case CEC_STATE_INITIATOR_DATA_HIGH:
		tr_inc_bit_offset(&cec_ts.tx);
		if (cec_ts.tx.bit == 0)
			enter_state(CEC_STATE_INITIATOR_EOM_LOW);
		else
			enter_state(CEC_STATE_INITIATOR_DATA_LOW);
		break;
	case CEC_STATE_FOLLOWER_START_LOW:
	case CEC_STATE_FOLLOWER_START_HIGH:
	case CEC_STATE_FOLLOWER_HEADER_INIT_LOW:
	case CEC_STATE_FOLLOWER_HEADER_INIT_HIGH:
	case CEC_STATE_FOLLOWER_HEADER_DEST_LOW:
	case CEC_STATE_FOLLOWER_HEADER_DEST_HIGH:
	case CEC_STATE_FOLLOWER_EOM_LOW:
	case CEC_STATE_FOLLOWER_EOM_HIGH:
	case CEC_STATE_FOLLOWER_ACK_FINISH:
	case CEC_STATE_FOLLOWER_DATA_LOW:
	case CEC_STATE_FOLLOWER_DATA_HIGH:
		enter_state(CEC_STATE_IDLE);
		break;
	case CEC_STATE_FOLLOWER_ACK_LOW:
		enter_state(CEC_STATE_FOLLOWER_ACK_FINISH);
		break;
	}
}

static void cec_ev_cap(void)
{
	int t;
	bool data;

	/* Something got captured. What edge and what it means is entirely state dependent */
	switch (cec_ts.state) {
	case CEC_STATE_IDLE:
		/* A falling edge during idle, likely a start bit */
		enter_state(CEC_STATE_FOLLOWER_START_LOW);
		break;
	case CEC_STATE_INITIATOR_FREE_TIME:
	case CEC_STATE_INITIATOR_START_HIGH:
	case CEC_STATE_INITIATOR_HEADER_INIT_HIGH:
		/* A falling edge during free-time, postpone this send and listen */
		cec_ts.tx.bit = 0;
		cec_ts.tx.byte = 0;
		enter_state(CEC_STATE_FOLLOWER_START_LOW);
		break;
	case CEC_STATE_FOLLOWER_START_LOW:
		/* Rising edge of start bit, validate low time */
		if (VALID_LOW_TIME(START_BIT, cap_get_time(cec_ts.cap_start)))
			enter_state(CEC_STATE_FOLLOWER_START_HIGH);
		else
			enter_state(CEC_STATE_IDLE);
		break;
	case CEC_STATE_FOLLOWER_START_HIGH:
		if (VALID_HIGH_TIME(START_BIT, cap_get_time(cec_ts.cap_start)))
			enter_state(CEC_STATE_FOLLOWER_HEADER_INIT_LOW);
		else
			enter_state(CEC_STATE_IDLE);
		break;
	case CEC_STATE_FOLLOWER_HEADER_INIT_LOW:
	case CEC_STATE_FOLLOWER_HEADER_DEST_LOW:
	case CEC_STATE_FOLLOWER_DATA_LOW:
		t = cap_get_time(cec_ts.cap_start);
		if (VALID_LOW_TIME(LOGICAL_ZERO, t))
			tr_set_data_bit(&cec_ts.rx, 0);
		else if (VALID_LOW_TIME(LOGICAL_ONE, t))
			tr_set_data_bit(&cec_ts.rx, 1);
		else
			enter_state(CEC_STATE_IDLE);

		enter_state(cec_ts.state + 1);
		break;
	case CEC_STATE_FOLLOWER_HEADER_INIT_HIGH:
		t = cap_get_time(cec_ts.cap_start);
		data = tr_get_data_bit(&cec_ts.rx);
		if ((data && VALID_HIGH_TIME(LOGICAL_ONE, t)) ||
		    (!data && VALID_HIGH_TIME(LOGICAL_ZERO, t))) {
			tr_inc_bit_offset(&cec_ts.rx);
			if (cec_ts.rx.bit == 4)
				enter_state(CEC_STATE_FOLLOWER_HEADER_DEST_LOW);
			else
				enter_state(CEC_STATE_FOLLOWER_HEADER_INIT_LOW);
		} else {
			enter_state(CEC_STATE_IDLE);
		}
		break;
	case CEC_STATE_FOLLOWER_HEADER_DEST_HIGH:
		t = cap_get_time(cec_ts.cap_start);
		data = tr_get_data_bit(&cec_ts.rx);
		if ((data && VALID_HIGH_TIME(LOGICAL_ONE, t)) ||
		    (!data && VALID_HIGH_TIME(LOGICAL_ZERO, t))) {
			tr_inc_bit_offset(&cec_ts.rx);
			if (cec_ts.rx.bit == 0) {
				/* TODO(sadolfsson): Check for my address or broadcast */
				enter_state(CEC_STATE_FOLLOWER_EOM_LOW);
			} else {
				enter_state(CEC_STATE_FOLLOWER_HEADER_DEST_LOW);
			}
		} else {
			enter_state(CEC_STATE_IDLE);
		}
		break;
	case CEC_STATE_FOLLOWER_EOM_LOW:
		t = cap_get_time(cec_ts.cap_start);
		if (VALID_LOW_TIME(LOGICAL_ZERO, t))
			cec_ts.eom = 0;
		else if (VALID_LOW_TIME(LOGICAL_ONE, t))
			cec_ts.eom = 1;
		else
			enter_state(CEC_STATE_IDLE);

		enter_state(CEC_STATE_FOLLOWER_EOM_HIGH);
		break;
	case CEC_STATE_FOLLOWER_EOM_HIGH:
		t = cap_get_time(cec_ts.cap_start);
		data = cec_ts.eom;
		if ((data && VALID_HIGH_TIME(LOGICAL_ONE, t)) ||
		    (!data && VALID_HIGH_TIME(LOGICAL_ZERO, t))) {
			enter_state(CEC_STATE_FOLLOWER_ACK_LOW);
		} else {
			enter_state(CEC_STATE_IDLE);
		}
		break;
	case CEC_STATE_FOLLOWER_ACK_LOW:
		enter_state(CEC_STATE_FOLLOWER_ACK_FINISH);
		break;
	case CEC_STATE_FOLLOWER_ACK_FINISH:
		enter_state(CEC_STATE_IDLE);
		break;
	case CEC_STATE_FOLLOWER_DATA_HIGH:
		t = cap_get_time(cec_ts.cap_start);
		data = tr_get_data_bit(&cec_ts.rx);
		if ((data && VALID_HIGH_TIME(LOGICAL_ONE, t)) ||
		    (!data && VALID_HIGH_TIME(LOGICAL_ZERO, t))) {
			if (cec_ts.rx.bit == 0)
				enter_state(CEC_STATE_FOLLOWER_EOM_LOW);
			else
				enter_state(CEC_STATE_FOLLOWER_DATA_LOW);
		} else {
			enter_state(CEC_STATE_IDLE);
		}
		break;
	default:
		break;
	}
}

static void cec_isr(void)
{
	int mdl = NPCX_MFT_MODULE_1;
	uint8_t events;

	/* Retrieve events NPCX_TECTRL_TAXND */
	events = GET_FIELD(NPCX_TECTRL(mdl), FIELD(0, 4));

#if DEBUG_CEC
	if (event_log_idx < EVENT_LOG_SIZE) {
		event_log[event_log_idx].state = cec_ts.state;
		event_log[event_log_idx].event = events;
		event_log[event_log_idx].tcra = NPCX_TCRA(mdl);
		event_log[event_log_idx].tcnt1 = NPCX_TCNT1(mdl);
		event_log[event_log_idx].tien = NPCX_TIEN(mdl);
		event_log[event_log_idx].cap_start = cec_ts.cap_start;
		event_log_idx++;
	}
#endif

	if (events & (1 << NPCX_TECTRL_TAPND)) {
		/* Capture event */
		cap_stop();
		cec_ev_cap();
	} else {
		/*
		 * Capture timeout
		 * We only care about this if the capture event is not happening,
		 * since we will get both events in the edge-trigger case
		 */
		if (events & (1 << NPCX_TECTRL_TCPND)) {
			cap_stop();
			cec_ev_tmo();
		}
	}
	/* Timer timeout */
	if (events & (1 << NPCX_TECTRL_TDPND)) {
		tmr_stop();
		cec_ev_tmo();
	}

	/* Clear events NPCX_TECTRL_TAXND  */
	SET_FIELD(NPCX_TECLR(mdl), FIELD(0, 4), events);
}
/* TODO(sadolfsson): Fix priority (lower than fan-rpm and usb-pd) */
DECLARE_IRQ(NPCX_IRQ_MFT_1, cec_isr, 1);

static int cec_send(const uint8_t *data, uint8_t len)
{
	int i;

	/* TODO(sadolfsson): Check busy */
	cec_ts.state = CEC_STATE_INITIATOR_FREE_TIME;
	cec_ts.tx_len = len;
	cec_ts.tx.bit = 0;
	cec_ts.tx.byte = 0;

	CPRINTS("Send CEC:");
	for (i = 0; i < len && i < MAX_CEC_MSG_LEN; i++) {
		cec_ts.tx.data[i] = data[i];
		CPRINTS(" 0x%02x", data[i]);
	}

	cec_ts.cap_start = FREE_TIME_NI;
	cap_start(CAP_EDGE_FALLING, FREE_TIME_NI);

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

#if DEBUG_CEC
int cecstates(int argc, char** argv)
{
	int i;
	int mdl = NPCX_MFT_MODULE_1;

	CPRINTF("tcra:  0x%04x\n", NPCX_TCRA(mdl));
	CPRINTF("tcnt1: 0x%04x\n", NPCX_TCNT1(mdl));
	CPRINTF("tien:  0x%04x\n", NPCX_TIEN(mdl));
	CPRINTF("tectrl:0x%04x\n", NPCX_TECTRL(mdl));
	CPRINTF("teckc: 0x%04x\n", NPCX_TCKC(mdl));

	CPRINTF("States (%d):\n", state_log_idx);
	for (i = 0; i < state_log_idx; i++) {
		CPRINTF("state: %d\n", state_log[i].state);
		CPRINTF("edge: %d\n", state_log[i].cap_edge);
		CPRINTF("cap: %d\n", state_log[i].cap_tmo);
		CPRINTF("tmr: %d\n", state_log[i].tmr_tmo);
	}
	return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(cecstates, cecstates, NULL, NULL);

int cecevents(int argc, char** argv)
{
	int i;

	CPRINTF("Events (%d):\n", event_log_idx);
	for (i = 0; i < event_log_idx; i++) {
		CPRINTF("state: %d\n", event_log[i].state);
		CPRINTF("event: 0x%04x\n", event_log[i].event);
		CPRINTF("tcra:  0x%04x\n", event_log[i].tcra);
		CPRINTF("tcnt1: 0x%04x\n", event_log[i].tcnt1);
		CPRINTF("tien:  0x%04x\n", event_log[i].tien);
		CPRINTF("caps:  0x%04x\n", event_log[i].cap_start);
		CPRINTF("cap_us:  %d\n", (int)APB1_US(event_log[i].cap_start - event_log[i].tcra));
	}
	return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(cecevents, cecevents, NULL, NULL);

#endif /* DEBUG_CEC */

static void cec_tmr_init(void)
{
	int mdl = NPCX_MFT_MODULE_1;

	/* Alt-setting for TA1.
	 * TODO(sadolfsson): Use gpio.inc instead
	 */
	CLEAR_BIT(NPCX_DEVALT(0xC), NPCX_DEVALTC_TA1_SL2);
	SET_BIT(NPCX_DEVALT(3), NPCX_DEVALT3_TA1_SL1);

	/* Ensure Multi-Function timer is powered up. */
	CLEAR_BIT(NPCX_PWDWN_CTL(mdl), NPCX_PWDWN_CTL1_MFT1_PD);

	/* Mode 2 - Dual-input capture */
	SET_FIELD(NPCX_TMCTRL(mdl), NPCX_TMCTRL_MDSEL_FIELD, NPCX_MFT_MDSEL_2);

	/* Enable capture TCNT1 into TCRA and preset TCNT1. */
	SET_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TAEN);

	/* TODO(sadolfsson): Should we really do low pwr? */
	SET_BIT(NPCX_TCKC(mdl), NPCX_TCKC_LOW_PWR);

	/* Enable interrupts */
	SET_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TAIEN);
	SET_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TDIEN);

	/* Reset clock counters. */
	NPCX_TCRA(mdl) = 0;
	NPCX_TCNT1(mdl) = 0xffff;
	NPCX_TCNT2(mdl) = 0;
}

static void cec_init(void)
{
	gpio_set_level(CEC_GPIO_PULL_UP, 1);
	gpio_set_level(CEC_GPIO_OUT, 1);

	apb1_freq = clock_get_apb1_freq();

	/* Initialize timers */
	cec_tmr_init();
	/* Capture falling edge of start bit */
	cap_start(CAP_EDGE_FALLING, 0);
	/* Enable multifunction timer interrupt */
	task_enable_irq(NPCX_IRQ_MFT_1);

	CPRINTS("CEC enabled.");
}
DECLARE_HOOK(HOOK_INIT, cec_init, HOOK_PRIO_LAST);
