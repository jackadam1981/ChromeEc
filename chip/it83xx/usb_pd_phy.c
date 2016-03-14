/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PD driver */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_phy_chip.h"
#include "usb_pd_tcpm.h"

#define TASK_EVENT_PHY_TX_DONE TASK_EVENT_CUSTOM((1 << 17))

/* cc pins */
#define USBPD_PD1CC1          IT83XX_GPIO_GPCRF4
#define USBPD_PD1CC2          IT83XX_GPIO_GPCRF5
#define USBPD_PD2CC1          IT83XX_GPIO_GPCRH1
#define USBPD_PD2CC2          IT83XX_GPIO_GPCRH2

#define USBPD_RX_INTERVAL_US   (2 * MSEC)
#ifndef USBPD_RX_RESTORE_US
#define USBPD_RX_RESTORE_US    (10 * MSEC)
#endif
static enum usbpd_port port_hard_rst;
static int rx_restore_us;

static void chip_pd_rx_restore(void)
{
	if (pd_snk_is_vbus_provided(port_hard_rst))
		rx_restore_us += USBPD_RX_INTERVAL_US;
	else
		rx_restore_us = 0;

	if (rx_restore_us >= USBPD_RX_RESTORE_US) {
		IT83XX_USBPD_GCR(port_hard_rst) |= USBPD_REG_MASK_BMC_PHY;
		rx_restore_us = 0;
		task_wake(PD_PORT_TO_TASK_ID(port_hard_rst));
	} else {
		hook_call_deferred(chip_pd_rx_restore, USBPD_RX_INTERVAL_US);
	}
}
DECLARE_DEFERRED(chip_pd_rx_restore);

/* 8320-defined, phy layer serve tcpc for register message */
enum tcpc_cc_voltage_status chip_pd_get_cc(
	enum usbpd_port port,
	enum usbpd_cc_pin cc_pin)
{
	enum usbpd_ufp_volt_status ufp_volt;
	enum usbpd_dfp_volt_status dfp_volt;
	enum tcpc_cc_voltage_status cc_state = TYPEC_CC_VOLT_OPEN;
	int pull;

	pull = (cc_pin == USBPD_CC_PIN_1) ?
		USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) :
		USBPD_GET_CC2_PULL_REGISTER_SELECTION(port);

	/* select Rp */
	if (pull)
		CLEAR_MASK(cc_state, (1 << 2));
	/* select Rd */
	else
		SET_MASK(cc_state, (1 << 2));

	/* sink */
	if (USBPD_GET_POWER_ROLE(port) == USBPD_POWER_ROLE_CONSUMER) {
		if (cc_pin == USBPD_CC_PIN_1)
			ufp_volt = IT83XX_USBPD_UFPVDR(port) & 0xf;
		else
			ufp_volt = (IT83XX_USBPD_UFPVDR(port) >> 4) & 0xf;

		switch (ufp_volt) {
		case USBPD_UFP_STATE_SNK_DEF:
			cc_state |= (TYPEC_CC_VOLT_SNK_DEF >> 2) & 3;
			break;
		case USBPD_UFP_STATE_SNK_1_5:
			cc_state |= (TYPEC_CC_VOLT_SNK_1_5 >> 2) & 3;
			break;
		case USBPD_UFP_STATE_SNK_3_0:
			cc_state |= (TYPEC_CC_VOLT_SNK_3_0 >> 2) & 3;
			break;
		case USBPD_UFP_STATE_SNK_OPEN:
			cc_state = TYPEC_CC_VOLT_OPEN;
			break;
		default:
			cc_state = TYPEC_CC_VOLT_OPEN;
			break;
		}
	/* source */
	} else {
		if (cc_pin == USBPD_CC_PIN_1)
			dfp_volt = IT83XX_USBPD_DFPVDR(port) & 0xf;
		else
			dfp_volt = (IT83XX_USBPD_DFPVDR(port) >> 4) & 0xf;

		switch (dfp_volt) {
		case USBPD_DFP_STATE_SRC_RA:
			cc_state |= TYPEC_CC_VOLT_RA;
			break;
		case USBPD_DFP_STATE_SRC_RD:
			cc_state |= TYPEC_CC_VOLT_RD;
			break;
		case USBPD_DFP_STATE_SRC_OPEN:
			cc_state = TYPEC_CC_VOLT_OPEN;
			break;
		default:
			cc_state = TYPEC_CC_VOLT_OPEN;
			break;
		}
	}

	return cc_state;
}

void chip_pd_rx_data(enum usbpd_port port, int *head, uint32_t *buf)
{
	struct usbpd_header *p_head = (struct usbpd_header *)head;
	*head = 0;

	if (!USBPD_IS_RX_DONE(port))
		return;

	/* store header */
	*p_head = *((struct usbpd_header *)IT83XX_USBPD_RMH_BASE(port));
	/* check data message */
	if (p_head->data_obj_num)
		memcpy(buf,
			(uint8_t *)IT83XX_USBPD_RDO_BASE(port),
			p_head->data_obj_num * 4);
	/*
	 * Note: clear RX done interrupt after get the data.
	 * If clear this bit, USBPD receives next packet
	 */
	IT83XX_USBPD_MRSR(port) = USBPD_REG_MASK_RX_MSG_VALID;
}

enum tcpc_transmit_complete chip_pd_tx_data(
	enum usbpd_port port,
	enum tcpm_transmit_type type,
	uint8_t msg_type,
	uint8_t length,
	const uint32_t *buf)
{
	int r;
	uint32_t evt;

	/* set message type */
	IT83XX_USBPD_MTSR0(port) =
		(IT83XX_USBPD_MTSR0(port) & ~0x1f) | (msg_type & 0xf);
	/* SOP type: bit[5:4] 00 SOP, 01 SOP', 10 SOP" */
	IT83XX_USBPD_MTSR1(port) =
		(IT83XX_USBPD_MTSR1(port) & ~0x30) | ((type & 0x3) << 4);
	/* bit7: transmit message is send to cable or not */
	if (TCPC_TX_SOP == type)
		IT83XX_USBPD_MTSR0(port) &= ~USBPD_REG_MASK_CABLE_ENABLE;
	else
		IT83XX_USBPD_MTSR0(port) |= USBPD_REG_MASK_CABLE_ENABLE;
	/* clear msg length */
	IT83XX_USBPD_MTSR1(port) &= (~0x7);
	/* Limited by PD_HEADER_CNT() */
	ASSERT(length <= 0x7);

	if (length) {
		/* set data bit */
		IT83XX_USBPD_MTSR0(port) |= (1 << 4);
		/* set data length setting */
		IT83XX_USBPD_MTSR1(port) |= length;
		/* set data */
		memcpy((uint8_t *)IT83XX_USBPD_TDO_BASE(port), buf, length * 4);
	}

	for (r = 0; r <= PD_RETRY_COUNT; r++) {
		/* Start TX */
		USBPD_KICK_TX_START(port);
		evt = task_wait_event_mask(TASK_EVENT_PHY_TX_DONE,
					PD_T_TCPC_TX_TIMEOUT);
		/* check TX status */
		if (USBPD_IS_TX_ERR(port) || (evt & TASK_EVENT_TIMER)) {
			/*
			 * If discard, means HW doesn't send the msg and resend.
			 */
			if (USBPD_IS_TX_DISCARD(port))
				continue;
			else
				return TCPC_TX_COMPLETE_FAILED;
		} else {
			break;
		}
	}

	if (r > PD_RETRY_COUNT)
		return TCPC_TX_COMPLETE_DISCARDED;

	return TCPC_TX_COMPLETE_SUCCESS;
}

enum tcpc_transmit_complete chip_pd_send_hw_reset(enum usbpd_port port,
				enum tcpm_transmit_type reset_type)
{
	if (reset_type == TCPC_TX_CABLE_RESET)
		IT83XX_USBPD_MTSR0(port) |= USBPD_REG_MASK_CABLE_ENABLE;
	else
		IT83XX_USBPD_MTSR0(port) &= ~USBPD_REG_MASK_CABLE_ENABLE;

	/* send hard reset */
	USBPD_SEND_HARD_RESET(port);
	usleep(MSEC);

	if (IT83XX_USBPD_MTSR0(port) & USBPD_REG_MASK_SEND_HW_RESET)
		return TCPC_TX_COMPLETE_FAILED;

	USBPD_DISABLE_BMC_PHY(port);
	port_hard_rst = port;
	rx_restore_us = 0;
	hook_call_deferred(chip_pd_rx_restore, USBPD_RX_INTERVAL_US);

	return TCPC_TX_COMPLETE_SUCCESS;
}

void chip_pd_send_bist_mode2_pattern(enum usbpd_port port)
{
	USBPD_ENABLE_SEND_BIST_MODE_2(port);
	usleep(PD_T_BIST_TRANSMIT);
	USBPD_DISABLE_SEND_BIST_MODE_2(port);
}

void chip_pd_enable_vconn(enum usbpd_port port, int enabled)
{
	enum usbpd_cc_pin cc_pin;

	if (USBPD_GET_PULL_CC_SELECTION(port))
		cc_pin = USBPD_CC_PIN_1;
	else
		cc_pin = USBPD_CC_PIN_2;

	if (enabled) {
		/* Disable unused CC to become VCONN */
		if (cc_pin == USBPD_CC_PIN_1) {
			IT83XX_USBPD_CCCSR(port) =
				(IT83XX_USBPD_CCCSR(port) | 0xa0) & ~0xa;
			IT83XX_USBPD_CCPSR(port) = (IT83XX_USBPD_CCPSR(port)
				& ~USBPD_REG_MASK_DISCONNECT_POWER_CC2)
				| USBPD_REG_MASK_DISCONNECT_POWER_CC1;
		} else {
			IT83XX_USBPD_CCCSR(port) =
				(IT83XX_USBPD_CCCSR(port) | 0xa) & ~0xa0;
			IT83XX_USBPD_CCPSR(port) = (IT83XX_USBPD_CCPSR(port)
				& ~USBPD_REG_MASK_DISCONNECT_POWER_CC1)
				| USBPD_REG_MASK_DISCONNECT_POWER_CC2;
		}
	} else {
		/* Enable cc1 and cc2 */
		IT83XX_USBPD_CCCSR(port) &= ~0xaa;
		IT83XX_USBPD_CCPSR(port) |=
			(USBPD_REG_MASK_DISCONNECT_POWER_CC1 |
			USBPD_REG_MASK_DISCONNECT_POWER_CC2);
	}
}

void chip_pd_set_power_role(enum usbpd_port port, int power_role)
{
	/* PD_ROLE_SINK 0, PD_ROLE_SOURCE 1 */
	if (power_role == PD_ROLE_SOURCE) {
		/* bit0: source */
		SET_MASK(IT83XX_USBPD_PDMSR(port), (1 << 0));
		/* bit1: CC1 select Rp */
		SET_MASK(IT83XX_USBPD_CCGCR(port), (1 << 1));
		/* bit3: CC2 select Rp */
		SET_MASK(IT83XX_USBPD_BMCSR(port), (1 << 3));
	} else {
		/* bit0: sink */
		CLEAR_MASK(IT83XX_USBPD_PDMSR(port), (1 << 0));
		/* bit1: CC1 select Rd */
		CLEAR_MASK(IT83XX_USBPD_CCGCR(port), (1 << 1));
		/* bit3: CC2 select Rd */
		CLEAR_MASK(IT83XX_USBPD_BMCSR(port), (1 << 3));
	}
}

void chip_pd_set_data_role(enum usbpd_port port, int pd_role)
{
	/* 0: PD_ROLE_UFP 1: PD_ROLE_DFP */
	IT83XX_USBPD_PDMSR(port) =
		(IT83XX_USBPD_PDMSR(port) & ~0xc) | ((pd_role & 0x1) << 2);
}

void chip_pd_init(enum usbpd_port port, int role)
{
	/* defalut PD Clock = PLL 48 / 6 = 8M. */
	IT83XX_ECPM_SCDCR4 = (IT83XX_ECPM_SCDCR4 & 0xf0) | 5;
	/* reset */
	IT83XX_USBPD_GCR(port) = 0;
	USBPD_SW_RESET(port);
	/* set SOP: receive SOP message only.
	 * bit[7]: SOP" support enable.
	 * bit[6]: SOP' support enable.
	 * bit[5]: SOP  support enable.
	 */
	IT83XX_USBPD_PDMSR(port) = USBPD_REG_MASK_SOP_ENABLE;
	/* W/C status */
	IT83XX_USBPD_ISR(port) = 0xff;
	/* enable cc, select cc1 and Rd (80uA output when Rp selected) */
	IT83XX_USBPD_CCGCR(port) = 0xd;
	/* change data role as the same power role */
	chip_pd_set_data_role(port, role);
	/* set power role */
	chip_pd_set_power_role(port, role);
	/* disable all interrupts */
	IT83XX_USBPD_IMR(port) = 0xff;
	/* enable tx done and reset detect interrupt */
	IT83XX_USBPD_IMR(port) &= ~(USBPD_REG_MASK_MSG_TX_DONE |
					USBPD_REG_MASK_HARD_RESET_DETECT);
	IT83XX_USBPD_CCPSR(port) = 0xff;
	/* cc connect */
	IT83XX_USBPD_CCCSR(port) = 0;
	/* disable vconn */
	chip_pd_enable_vconn(port, 0);
	/* TX start from high */
	IT83XX_USBPD_CCADCR(port) |= (1 << 6);
	/* enable cc1/cc2 */
	if (port == USBPD_PORT_A) {
		USBPD_PD1CC1 = 0xc0;
		USBPD_PD1CC2 = 0xc0;
		task_clear_pending_irq(IT83XX_IRQ_USBPD0);
		task_enable_irq(IT83XX_IRQ_USBPD0);
	} else {
		USBPD_PD2CC1 = 0xc0;
		USBPD_PD2CC2 = 0xc0;
		task_clear_pending_irq(IT83XX_IRQ_USBPD1);
		task_enable_irq(IT83XX_IRQ_USBPD1);
	}
	USBPD_START(port);
}

void chip_pd_select_polarity(enum usbpd_port port, enum usbpd_cc_pin cc_pin)
{
	/* cc1/cc2 selection */
	if (cc_pin == USBPD_CC_PIN_1)
		SET_MASK(IT83XX_USBPD_CCGCR(port), (1 << 0));
	else
		CLEAR_MASK(IT83XX_USBPD_CCGCR(port), (1 << 0));
}

void chip_pd_set_cc(enum usbpd_port port, int pull)
{
	if (pull == TYPEC_CC_RD)
		chip_pd_set_power_role(port, PD_ROLE_SINK);
	else if (pull == TYPEC_CC_RP)
		chip_pd_set_power_role(port, PD_ROLE_SOURCE);
}

void chip_pd_irq(enum usbpd_port port)
{
	if (port == USBPD_PORT_A)
		task_clear_pending_irq(IT83XX_IRQ_USBPD0);
	else if (port == USBPD_PORT_B)
		task_clear_pending_irq(IT83XX_IRQ_USBPD1);

	/* check status */
	if (USBPD_IS_HARD_RESET_DETECT(port)) {
		/* clear interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_HARD_RESET_DETECT;
		task_set_event(PD_PORT_TO_TASK_ID(port),
			PD_EVENT_TCPC_RESET, 0);
	} else {
		if (USBPD_IS_RX_DONE(port)) {
			/* mask RX done interrupt */
			IT83XX_USBPD_IMR(port) |= USBPD_REG_MASK_MSG_RX_DONE;
			/* clear RX done interrupt */
			IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_RX_DONE;
			task_set_event(PD_PORT_TO_TASK_ID(port),
				PD_EVENT_RX, 0);
		}
		if (USBPD_IS_TX_DONE(port)) {
			/* clear TX done interrupt */
			IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
			task_set_event(PD_PORT_TO_TASK_ID(port),
				TASK_EVENT_PHY_TX_DONE, 0);
		}
	}
}
