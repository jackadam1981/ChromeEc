/* Copyright 2018 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cec.h"
#include "clock_chip.h"
#include "console.h"
#include "driver/cec/bitbang.h"
#include "fan_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#if !(DEBUG_CEC)
#define CPRINTF(...)
#define CPRINTS(...)
#else
#define CPRINTF(format, args...) cprintf(CC_CEC, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CEC, format, ##args)
#endif

struct npcx_tmr_flags {
	uint8_t interrupt;
	uint8_t pending;
	uint8_t clear_pending;
};

struct npcx_cec_port {
	/*
	 * Time between interrupt triggered and the next timer was
	 * set when measuring pulse width
	 */
	int cap_delay;

	/* Value charged into the capture timer on last capture start */
	int cap_charge;

	volatile uint16_t *tcr;
	volatile uint16_t *tcnt;
	struct npcx_tmr_flags capture;
	struct npcx_tmr_flags underflow;
	uint8_t clock_select;
	uint8_t edge;

	/* Software generated interrupt triggers the send logic */
	uint8_t sw_interrupt;
};

static struct npcx_cec_port npcx_cec_port[CEC_PORT_COUNT];

/* APB1 frequency. Store divided by 10k to avoid some runtime divisions */
uint32_t apb1_freq_div_10k;

void cec_tmr_cap_start(int port, enum cec_cap_edge edge, int timeout)
{
	int mdl = NPCX_MFT_MODULE_1;
	struct npcx_cec_port *cec_port = &npcx_cec_port[port];
	const struct npcx_tmr_flags *capture_flags = &cec_port->capture;
	const struct npcx_tmr_flags *underflow_flags = &cec_port->underflow;

	if (edge == CEC_CAP_EDGE_NONE) {
		/*
		 * If edge is NONE, disable capture interrupts and wait for a
		 * timeout only.
		 */
		CLEAR_BIT(NPCX_TIEN(mdl), capture_flags->interrupt);
	} else {
		/* Select edge to trigger capture on */
		UPDATE_BIT(NPCX_TMCTRL(mdl), cec_port->edge,
			   edge == CEC_CAP_EDGE_RISING);
		SET_BIT(NPCX_TIEN(mdl), capture_flags->interrupt);
	}

	/*
	 * Set capture timeout. If we don't have a timeout, we
	 * turn the timeout interrupt off and only care about
	 * the edge change.
	 */
	if (timeout > 0) {
		/*
		 * Store the time it takes from the interrupts starts to when we
		 * actually get here. This part of the pulse-width needs to be
		 * taken into account
		 */
		cec_port->cap_delay = (0xffff - *cec_port->tcnt);
		cec_port->cap_charge = timeout - cec_port->cap_delay;
		*cec_port->tcnt = cec_port->cap_charge;
		SET_BIT(NPCX_TIEN(mdl), underflow_flags->interrupt);
	} else {
		CLEAR_BIT(NPCX_TIEN(mdl), underflow_flags->interrupt);
		*cec_port->tcnt = 0;
	}

	/* Clear out old events */
	SET_BIT(NPCX_TECLR(mdl), capture_flags->clear_pending);
	SET_BIT(NPCX_TECLR(mdl), underflow_flags->clear_pending);
	*cec_port->tcr = 0;

	/* Start the capture timer */
	if (cec_port->clock_select == 1)
		SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C1CSEL_FIELD, 1);
	else if (cec_port->clock_select == 2)
		SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C2CSEL_FIELD, 1);
}

void cec_tmr_cap_stop(int port)
{
	int mdl = NPCX_MFT_MODULE_1;
	const struct npcx_cec_port *cec_port = &npcx_cec_port[port];

	CLEAR_BIT(NPCX_TIEN(mdl), NPCX_TIEN_TCIEN);
	if (cec_port->clock_select == 1)
		SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C1CSEL_FIELD, 0);
	else if (cec_port->clock_select == 2)
		SET_FIELD(NPCX_TCKC(mdl), NPCX_TCKC_C2CSEL_FIELD, 0);
}

int cec_tmr_cap_get(int port)
{
	const struct npcx_cec_port *cec_port = &npcx_cec_port[port];

	return (cec_port->cap_charge + cec_port->cap_delay - *cec_port->tcr);
}

static void cec_isr(void)
{
	int mdl = NPCX_MFT_MODULE_1;
	uint8_t events;

	/* Retrieve events NPCX_TECTRL_TAXND */
	events = GET_FIELD(NPCX_TECTRL(mdl), FIELD(0, 4));

	for (int port = 0; port < CEC_PORT_COUNT; port++) {
		if (events & BIT(npcx_cec_port[port].capture.pending)) {
			/* Capture event */
			cec_event_cap(port);
		} else if (events &
			   BIT(npcx_cec_port[port].underflow.pending)) {
			/*
			 * Capture timeout
			 * We only care about this if the capture event is not
			 * happening, since we will get both events in the
			 * edge-trigger case
			 */
			cec_event_timeout(port);
		}
	}

	/* Software interrupt, a transfer has been initiated from AP */
	for (int port = 0; port < CEC_PORT_COUNT; port++) {
		if (npcx_cec_port[port].sw_interrupt > 0) {
			npcx_cec_port[port].sw_interrupt = 0;
			cec_event_tx(port);
		}
	}

	/* Clear handled events */
	SET_FIELD(NPCX_TECLR(mdl), FIELD(0, 4), events);
}
DECLARE_IRQ(NPCX_IRQ_MFT_1, cec_isr, 4);

void cec_trigger_send(int port)
{
	struct npcx_cec_port *cec_port = &npcx_cec_port[port];

	/* Elevate to interrupt context */
	cec_port->sw_interrupt = 1;
	task_trigger_irq(NPCX_IRQ_MFT_1);
}

void cec_enable_timer(int port)
{
	int mdl = NPCX_MFT_MODULE_1;
	struct npcx_cec_port *cec_port = &npcx_cec_port[port];

	if (port == 0) {
		/* Configure GPIO40/TA1 as capture timer input (TA1) */
		CLEAR_BIT(NPCX_DEVALT(0xC), NPCX_DEVALTC_TA1_SL2);
		SET_BIT(NPCX_DEVALT(3), NPCX_DEVALT3_TA1_SL1);
	} else {
		/* Configure GPIOD3/TB1 as capture timer input (TB1) */
		CLEAR_BIT(NPCX_DEVALT(3), NPCX_DEVALT3_TB1_SL1);
		SET_BIT(NPCX_DEVALT(0xC), NPCX_DEVALTC_TB1_SL2);
	}

	/* Enable timer interrupts */
	SET_BIT(NPCX_TIEN(mdl), cec_port->capture.interrupt);

	/* Enable multifunction timer interrupt */
	task_enable_irq(NPCX_IRQ_MFT_1);
}

void cec_disable_timer(int port)
{
	int mdl = NPCX_MFT_MODULE_1;
	struct npcx_cec_port *cec_port = &npcx_cec_port[port];

	/* Disable timer interrupts */
	CLEAR_BIT(NPCX_TIEN(mdl), cec_port->capture.interrupt);

	cec_tmr_cap_stop(port);

	if (port == 0) {
		/* Configure GPIO40/TA1 back to GPIO */
		CLEAR_BIT(NPCX_DEVALT(3), NPCX_DEVALT3_TA1_SL1);
		SET_BIT(NPCX_DEVALT(0xC), NPCX_DEVALTC_TA1_SL2);
	} else {
		/* Configure GPIOD3/TB1 back to GPIO */
		CLEAR_BIT(NPCX_DEVALT(0xC), NPCX_DEVALTC_TB1_SL2);
		SET_BIT(NPCX_DEVALT(3), NPCX_DEVALT3_TB1_SL1);
	}

	cec_port->cap_charge = 0;
	cec_port->cap_delay = 0;

	/* If there is no enabled timers, turn off interrupts */
	for (int p = 0; p < CEC_PORT_COUNT; p++) {
		if (IS_BIT_SET(NPCX_TIEN(mdl),
			       npcx_cec_port[p].capture.interrupt))
			return;
	}
	task_disable_irq(NPCX_IRQ_MFT_1);
}

void cec_init_timer(int port)
{
	int mdl = NPCX_MFT_MODULE_1;
	struct npcx_cec_port *cec_port;
	struct npcx_tmr_flags *capture, *underflow;

	if (port < 0 || port >= CEC_PORT_COUNT) {
		CPRINTS("CEC ERR: Invalid port # %d", port);
		return;
	}

	if (port > 2) {
		CPRINTS("CEC ERR: NPCX does not support port # %d", port);
		return;
	}

	cec_port = &npcx_cec_port[port];
	capture = &cec_port->capture;
	underflow = &cec_port->underflow;
	if (port == 0) {
		/* Source A is capture and source C is underflow */
		cec_port->tcr = &NPCX_TCRA(mdl);
		cec_port->tcnt = &NPCX_TCNT1(mdl);
		cec_port->clock_select = 1;
		cec_port->edge = NPCX_TMCTRL_TAEDG;
		capture->interrupt = NPCX_TIEN_TAIEN;
		capture->pending = NPCX_TECTRL_TAPND;
		capture->clear_pending = NPCX_TECLR_TACLR;
		underflow->interrupt = NPCX_TIEN_TCIEN;
		underflow->pending = NPCX_TECTRL_TCPND;
		underflow->clear_pending = NPCX_TECLR_TCCLR;
	} else {
		/* Source B is capture and source D is underflow */
		cec_port->tcr = &NPCX_TCRB(mdl);
		cec_port->tcnt = &NPCX_TCNT2(mdl);
		cec_port->clock_select = 2;
		cec_port->edge = NPCX_TMCTRL_TBEDG;
		capture->interrupt = NPCX_TIEN_TBIEN;
		capture->pending = NPCX_TECTRL_TBPND;
		capture->clear_pending = NPCX_TECLR_TBCLR;
		underflow->interrupt = NPCX_TIEN_TDIEN;
		underflow->pending = NPCX_TECTRL_TDPND;
		underflow->clear_pending = NPCX_TECLR_TDCLR;
	}

	/* APB1 is the clock we base the timers on */
	apb1_freq_div_10k = clock_get_apb1_freq() / 10000;

	/* Ensure Multi-Function timer is powered up. */
	CLEAR_BIT(NPCX_PWDWN_CTL(mdl), NPCX_PWDWN_CTL1_MFT1_PD);

	/* Mode 5 - Dual-independent input capture */
	SET_FIELD(NPCX_TMCTRL(mdl), NPCX_TMCTRL_MDSEL_FIELD, NPCX_MFT_MDSEL_5);

	if (port == 0) {
		/* Enable capture TCNT1 into TCRA and preset TCNT1. */
		SET_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TAEN);
	} else {
		/* Enable capture TCNT2 into TCRB and preset TCNT2. */
		SET_BIT(NPCX_TMCTRL(mdl), NPCX_TMCTRL_TBEN);
	}
}
