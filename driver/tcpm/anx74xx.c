/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Author : Analogix Semiconductor.
 */

/* Type-C port manager for Analogix's anx74xx chips */

#include "anx74xx.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpc.h"
#include "util.h"

/* Defined in board.c */
void board_set_tcpc_power_mode(int port, int normal_mode);

struct anx_state {
	int	polarity;
	int	pull;
	int	vconn_en;
	int	mux_state;
	int	pin_mode;
};
static struct anx_state anx[CONFIG_USB_PD_PORT_COUNT];

static void tcpm_set_auto_good_crc(int port, int enable)
{
	int reg;

	/* Set default header for Good CRC auto reply */
	tcpc_read(port, TCPC_REG_TX_MSG_HEADER, &reg);
	reg |= (PD_REV20 << TCPC_REG_SPEC_REV_BIT_POS);
	reg |= TCPC_REG_AUTO_GOODCRC_EN;
	tcpc_write(port, TCPC_REG_TX_MSG_HEADER, reg);

	reg = TCPC_REG_ENABLE_GOODCRC;
	tcpc_write(port, TCPC_REG_TX_AUTO_GOODCRC_2, reg);

	tcpc_read(port, TCPC_REG_TX_AUTO_GOODCRC_1, &reg);

	/* Set bit-0 if enable, reset bit-0 if disable */
	if (enable)
		reg |= TCPC_REG_AUTO_GOODCRC_EN;
	else
		reg &= ~TCPC_REG_AUTO_GOODCRC_EN;

	tcpc_write(port, TCPC_REG_TX_AUTO_GOODCRC_1, reg);
}

static void anx74xx_set_power_mode(int port, int mode)
{
	switch (mode) {
		case TCPC_NORMAL_MODE:
			/* Set PWR_EN and RST_N GPIO pins high */
			board_set_tcpc_power_mode(port, 1);
			break;
		case TCPC_STANDBY_MODE:
			/* Disable PWR_EN, keep Digital and analog block
			 * ON for cable detection */
			board_set_tcpc_power_mode(port, 0);
			break;
		default:
			break;
	}
}

void tcpc_set_vbus(int port, int enable)
{
	int reg;

	tcpc_read(port, TCPC_REG_GPIO_CTRL_4_5, &reg);
	if (enable)
		reg |= TCPC_REG_SET_VBUS;
	else
		reg &= ~TCPC_REG_SET_VBUS;
	tcpc_write(port, TCPC_REG_GPIO_CTRL_4_5, reg);
}

void tcpc_update_hpd_status(int port, int hpd_lvl, int hpd_irq)
{
	int reg;

	tcpc_read(port, TCPC_REG_HPD_CTRL_0, &reg);
	if (hpd_lvl)
		reg |= TCPC_REG_HPD_OUT_DATA;
	else
		reg &= ~TCPC_REG_HPD_OUT_DATA;
	tcpc_write(port, TCPC_REG_HPD_CTRL_0, reg);

	if (hpd_irq) {
		tcpc_read(port, TCPC_REG_HPD_CTRL_0, &reg);
		reg &= ~TCPC_REG_HPD_OUT_DATA;
		tcpc_write(port, TCPC_REG_HPD_CTRL_0, reg);
		msleep(1);
		reg |= TCPC_REG_HPD_OUT_DATA;
                tcpc_write(port, TCPC_REG_HPD_CTRL_0, reg);
	}
}

void tcpc_set_dp_pin_mode(int port, int pin_mode)
{
	anx[port].pin_mode = pin_mode;
}

#ifdef CONFIG_USB_PD_TCPM_MUX
static int tcpm_mux_init(int i2c_addr)
{
	int port = i2c_addr;

	/* Nothing to do here, ANX initializes its muxes
	 * as (MUX_USB_ENABLED | MUX_DP_ENABLED)
	 */
	anx[port].mux_state = MUX_USB_ENABLED | MUX_DP_ENABLED;

	return EC_SUCCESS;
}

static int tcpm_mux_set(int i2c_addr, mux_state_t mux_state)
{
	int reg = 0, val = 0;
	int rv;
	int port = i2c_addr;

	if (mux_state & (MUX_USB_ENABLED|MUX_DP_ENABLED)) {
		rv = tcpc_read(port, TCPC_REG_ANALOG_CTRL_5, &reg);
		if (rv)
			return EC_ERROR_UNKNOWN;
		reg &= 0x0f;
		switch (anx[port].pin_mode) {
			case MODE_DP_PIN_A:
			case MODE_DP_PIN_C:
			case MODE_DP_PIN_E:
				if (anx[port].polarity) {
					val = TCPC_REG_MUX_DP_MODE_ACE_CC2;
					reg |= TCPC_REG_MUX_ML2_B;
				} else {
					val = TCPC_REG_MUX_DP_MODE_ACE_CC1;
					reg |= TCPC_REG_MUX_ML2_A;
				}
				break;
			case MODE_DP_PIN_B:
			case MODE_DP_PIN_D:
			case MODE_DP_PIN_F:
				if (anx[port].polarity) {
					val = TCPC_REG_MUX_DP_MODE_BDF_CC2;
					reg |= TCPC_REG_MUX_SSTX_B;
				} else {
					val = TCPC_REG_MUX_DP_MODE_BDF_CC1;
					reg |= TCPC_REG_MUX_SSTX_A;
				}
				break;

		}
		rv = tcpc_write(port, TCPC_REG_ANALOG_CTRL_1, val);
		rv |= tcpc_write(port, TCPC_REG_ANALOG_CTRL_5, reg);
		anx[port].mux_state = mux_state;
	} else {
		rv = EC_ERROR_UNIMPLEMENTED;
	}
	return rv;
}

/* Reads control register and updates mux_state accordingly */
static int tcpm_mux_get(int i2c_addr, mux_state_t *mux_state)
{
	int port = i2c_addr;

	*mux_state = anx[port].mux_state;

	return EC_SUCCESS;
}

const struct usb_mux_driver tcpm_usb_mux_driver = {
	.init = tcpm_mux_init,
	.set = tcpm_mux_set,
	.get = tcpm_mux_get,
};

#endif /* CONFIG_USB_PD_TCPM_MUX */

static int anx74xx_init_analog(int port)
{
	int reg, rv = EC_SUCCESS;

	/* Analog settings for chip */
        rv |= tcpc_write(port, TCPC_REG_HPD_CONTROL,
			 TCPC_REG_HPD_OP_MODE);
        rv |= tcpc_write(port, TCPC_REG_HPD_CTRL_0,
			 TCPC_REG_HPD_DEFAULT);
	if (rv)
		return rv;
	rv = tcpc_read(port, TCPC_REG_GPIO_CTRL_4_5, &reg);
	if (rv)
		return rv;
	reg &= TCPC_REG_VBUS_GPIO_MODE;
	reg |= TCPC_REG_VBUS_OP_ENABLE;
	rv = tcpc_write(port, TCPC_REG_GPIO_CTRL_4_5, reg);
	if (rv)
		return rv;
	rv = tcpc_read(port, TCPC_REG_CC_SOFTWARE_CTRL, &reg);
	if(rv)
		return rv;
	reg |= TCPC_REG_TX_MODE_ENABLE;
	rv = tcpc_write(port, TCPC_REG_CC_SOFTWARE_CTRL, reg);

	return rv;
}

static int anx74xx_set_mux(int port, int polarity)
{
	int reg, rv = EC_SUCCESS;

	rv = tcpc_read(port, TCPC_REG_ANALOG_CTRL_2, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	if (polarity) {
		reg |= TCPC_REG_AUX_SWAP_SET_CC2;
		reg &= ~TCPC_REG_AUX_SWAP_SET_CC1;
	} else {
		reg |= TCPC_REG_AUX_SWAP_SET_CC1;
		reg &= ~TCPC_REG_AUX_SWAP_SET_CC2;
	}
	rv = tcpc_write(port, TCPC_REG_ANALOG_CTRL_2, reg);

	return rv;
}

static int anx74xx_send_message(int port, uint16_t header,
				const uint32_t *payload,
				int type,
				unsigned char len)
{
	int reg, rv = EC_SUCCESS;
	unsigned char *buf = NULL;
	int num_retry = 0;

	/* Clear TX FIFO, toggle bit-0 */
	rv |= tcpc_read(port, TCPC_REG_TX_FIFO_CTRL, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	reg |= 0x01;
	rv |= tcpc_write(port, TCPC_REG_TX_FIFO_CTRL, reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	reg &= ~0x01;
	rv |= tcpc_write(port, TCPC_REG_TX_FIFO_CTRL, reg);

	/* Inform chip about message length and TX type
	 * type->bit-0..2, len->bit-3..7
	 */
	reg = (len << 3) & 0xf8;
	reg |= type & 0x07;
	rv |= tcpc_write(port, TCPC_REG_TX_CTRL_2, reg);
	if (rv)
		return EC_ERROR_UNKNOWN;

	/* Enqueue Header */
	buf = (unsigned char *)&header;
	rv = tcpc_write(port, TCPC_REG_TX_WR_FIFO, *buf);
	if (rv)
		return EC_ERROR_UNKNOWN;
	buf++;
	rv = tcpc_write(port, TCPC_REG_TX_WR_FIFO, *buf);
	if (rv)
		return EC_ERROR_UNKNOWN;

	/* Enqueue payload */
	len -= 2;
	buf = (unsigned char *)payload;
	while (1) {
		rv |= tcpc_write(port, TCPC_REG_TX_WR_FIFO, *buf);
		if (rv) {
			num_retry++;
		} else {
			buf++;
			len--;
			num_retry = 0;
		}
		if (len == 0 || num_retry >= 3)
			break;
	}
	/* If enqueue failed, do not request anx to transmit
	 * messages, FIFO will get cleared in next call
	 * before enqueue.
	 * num_retry = 0, refer to success
	 */
	if (num_retry)
		return EC_ERROR_UNKNOWN;

	/* Request a data transmission
	 * This bit will be cleared by ANX after TX success
	 */
	rv = tcpc_read(port, TCPC_REG_TX_CTRL_1, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	reg |= TCPC_REG_TX_SEND_DATA_REQ;
	rv |= tcpc_write(port, TCPC_REG_TX_CTRL_1, reg);

	return rv;
}

static int anx74xx_read_pd_obj(int port,
				unsigned char *buf,
				int plen)
{
	int rv = EC_SUCCESS, i;
	int reg, addr = TCPC_REG_PD_RX_DATA_OBJ;

	/* Read PD data objects from ANX */
	for (i = 0; i < plen ; i++) {
		/* Register sequence changes for last two bytes, if
		 * plen is greater than 26
		 */
		if (i == 26)
			addr = TCPC_REG_PD_RX_DATA_OBJ_M;
		rv = tcpc_read(port, addr + i, &reg);
		if(rv)
			break;
		buf[i] = reg;
	}

	/* Clear receive message interrupt bit(bit-0) */
	rv |= tcpc_read(port, TCPC_REG_IRQ_SOURCE_RECV_MSG, &reg);
	rv |= tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG,
			 reg & (~0x01));

	return rv;
}

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	int rv = EC_SUCCESS;
	int reg = 0;
	int status = 0;

	rv |= tcpc_read(port, TCPC_REG_ANALOG_STATUS, &reg);

	if (!anx[port].pull) {/* get CC in sink mode */
		rv |= tcpc_read(port, TCPC_REG_POWER_DOWN_CTRL, &reg);
		if (rv)
			return EC_ERROR_UNKNOWN;
		/* CC1 */
		if (reg & TCPC_REG_STATUS_CC1_VRD_USB)
			*cc1 = TYPEC_CC_VOLT_SNK_DEF;
		else if (reg & TCPC_REG_STATUS_CC1_VRD_1P5)
			*cc1 = TYPEC_CC_VOLT_SNK_1_5;
		else if (reg & TCPC_REG_STATUS_CC1_VRD_3P0)
			*cc1 = TYPEC_CC_VOLT_SNK_3_0;
		else
			*cc1 = TYPEC_CC_VOLT_OPEN;
		/* CC2 */
		if (reg & TCPC_REG_STATUS_CC2_VRD_USB)
			*cc2 = TYPEC_CC_VOLT_SNK_DEF;
		else if (reg & TCPC_REG_STATUS_CC2_VRD_1P5)
			*cc2 = TYPEC_CC_VOLT_SNK_1_5;
		else if (reg & TCPC_REG_STATUS_CC2_VRD_3P0)
			*cc2 = TYPEC_CC_VOLT_SNK_3_0;
		else
			*cc2 = TYPEC_CC_VOLT_OPEN;
	} else {/* get CC in source mode */
		rv |= tcpc_read(port, TCPC_REG_ANALOG_CTRL_7, &reg);
		if (rv)
			return EC_ERROR_UNKNOWN;
		/* CC1 */
		status = TCPC_REG_STATUS_CC1(reg);
		if (status) {
			if ((status & TCPC_REG_STATUS_CC_RA) ==
			    TCPC_REG_STATUS_CC_RA)
				*cc1 = TYPEC_CC_VOLT_RA;
			else if (status & TCPC_REG_STATUS_CC_RD)
				*cc1 = TYPEC_CC_VOLT_RD;
		} else {
				*cc1 = TYPEC_CC_VOLT_OPEN;
		}
		/* CC2 */
		status = TCPC_REG_STATUS_CC2(reg);
		if (status) {
			if ((status & TCPC_REG_STATUS_CC_RA) ==
			    TCPC_REG_STATUS_CC_RA)
				*cc2 = TYPEC_CC_VOLT_RA;
			else if (status & TCPC_REG_STATUS_CC_RD)
				*cc2 = TYPEC_CC_VOLT_RD;
		} else {
				*cc2 = TYPEC_CC_VOLT_OPEN;
		}
	}

	/* clear HPD status*/
	if(!(*cc1) && !(*cc2)) {
		tcpc_read(port, TCPC_REG_HPD_CTRL_0, &reg);
		reg &= 0xcf;
		tcpc_write(port, TCPC_REG_HPD_CTRL_0, reg);
	}

	return EC_SUCCESS;
}

int tcpm_set_cc(int port, int pull)
{
	int rv = EC_SUCCESS;
	int reg;

	/* Enable CC software Control */
	rv |= tcpc_read(port, TCPC_REG_CC_SOFTWARE_CTRL, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	reg |= TCPC_REG_CC_SW_CTRL_ENABLE;
	rv |= tcpc_write(port, TCPC_REG_CC_SOFTWARE_CTRL, reg);
	if (rv)
		return EC_ERROR_UNKNOWN;

	switch (pull) {
		case TYPEC_CC_RP:
			/* Enable Rp */
			rv |= tcpc_read(port, TCPC_REG_ANALOG_STATUS, &reg);
			if (rv)
				return EC_ERROR_UNKNOWN;
			reg |= TCPC_REG_CC_PULL_RP;
			rv |= tcpc_write(port, TCPC_REG_ANALOG_STATUS, reg);
			anx[port].pull = 1;
			break;
		case TYPEC_CC_RD:
			/* Enable Rd */
			rv |= tcpc_read(port, TCPC_REG_ANALOG_STATUS, &reg);
			if (rv)
				return EC_ERROR_UNKNOWN;
			reg &= TCPC_REG_CC_PULL_RD;
			rv |= tcpc_write(port, TCPC_REG_ANALOG_STATUS, reg);
			anx[port].pull = 0;
			break;
		default:
			rv = EC_ERROR_UNKNOWN;
			break;
	}

	return rv;
}

int tcpm_set_polarity(int port, int polarity)
{
	int reg, rv = EC_SUCCESS;

	rv |= tcpc_read(port, TCPC_REG_CC_SOFTWARE_CTRL, &reg);
	if (polarity) /* Inform ANX to use CC2 */
		reg &= ~TCPC_REG_SELECT_CC1;
	else /* Inform ANX to use CC1 */
		reg |= TCPC_REG_SELECT_CC1;
	rv |= tcpc_write(port, TCPC_REG_CC_SOFTWARE_CTRL, reg);

	anx[port].polarity = polarity;
	rv |= anx74xx_set_mux(port, polarity);

	/* Default DP pin mux D */
	tcpc_set_dp_pin_mode(port, MODE_DP_PIN_D);
#ifdef CONFIG_USB_PD_TCPM_MUX
	tcpm_mux_set(port, MUX_USB_ENABLED|MUX_DP_ENABLED);
#endif
	return rv;
}

int tcpm_set_vconn(int port, int enable)
{
	int reg, rv = EC_SUCCESS;

	/* switch VCONN to Non CC line */
	rv |= tcpc_read(port, TCPC_REG_INTP_VCONN_CTRL, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	if (enable) {
		if (anx[port].polarity)
			reg |= TCPC_REG_VCONN_1_ENABLE;
		else
			reg |= TCPC_REG_VCONN_2_ENABLE;
	} else {
		reg &= TCPC_REG_VCONN_DISABLE;
	}
	rv |= tcpc_write(port, TCPC_REG_INTP_VCONN_CTRL, reg);
	anx[port].vconn_en = enable;

	return rv;
}

int tcpm_set_msg_header(int port, int power_role, int data_role)
{
	int rv = 0, reg;

	rv |= tcpc_read(port, TCPC_REG_TX_MSG_HEADER, &reg);
	reg |= ((!!power_role) << TCPC_REG_PWR_ROLE_BIT_POS);
	reg |= ((!!data_role) << TCPC_REG_DATA_ROLE_BIT_POS);
	rv |= tcpc_write(port, TCPC_REG_TX_MSG_HEADER, reg);

	return rv;
}

static int anx74xx_alert_status(int port, int *alert)
{
	int reg, rv = EC_SUCCESS;

	/* Clear soft irq bit */
	rv |= tcpc_write(port, TCPC_REG_IRQ_EXT_SOURCE_2,
			 TCPC_REG_CLEAR_SOFT_IRQ);
	/* Read TCPC Alert registers */
	rv |= tcpc_read(port, TCPC_REG_IRQ_EXT_SOURCE_1, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;
	/* Clears interrupt bits */
	rv |= tcpc_write(port, TCPC_REG_IRQ_EXT_SOURCE_1, reg);

	*alert = reg;
	rv = tcpc_read(port, TCPC_REG_IRQ_SOURCE_RECV_MSG, &reg);
	if (rv)
		return EC_ERROR_UNKNOWN;

	if (reg & TCPC_REG_IRQ_CC_MSG_INT)
		*alert |= TCPC_REG_ALERT_MSG_RECV;
	else
		*alert &= (~TCPC_REG_ALERT_MSG_RECV);

	if (reg & TCPC_REG_IRQ_CC_STATUS_INT) {
		*alert |= TCPC_REG_ALERT_CC_CHANGE;
		rv |= tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG,
                          reg & 0xfd);
	} else {
		*alert &= (~TCPC_REG_ALERT_CC_CHANGE);
	}

	if (reg & TCPC_REG_IRQ_GOOD_CRC_INT) {
		*alert |= TCPC_REG_ALERT_TX_ACK_RECV;
		rv |= tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG,
                          reg & 0xfb);
	} else {
		*alert &= (~TCPC_REG_ALERT_TX_ACK_RECV);
	}

	return rv;
}

int tcpm_set_rx_enable(int port, int enable)
{
	int reg, rv = 0;

	rv |= tcpc_read(port, TCPC_REG_IRQ_SOURCE_RECV_MSG_MASK, &reg);
	if (enable)
		reg &= ~(TCPC_REG_IRQ_CC_MSG_INT);
	else/* Disable RX message by masking interrupt */
		reg |= (TCPC_REG_IRQ_CC_MSG_INT);

	tcpm_set_auto_good_crc(port, enable);
	rv |= tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG_MASK, reg);

	return rv;
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
int tcpm_get_vbus_level(int port)
{
	int reg = 0;

	tcpc_read(port, TCPC_REG_ANALOG_STATUS, &reg);
	return ((reg & TCPC_REG_VBUS_STATUS) ? 1 : 0);
}
#endif

int tcpm_get_message(int port, uint32_t *payload, int *head)
{
	int reg = 0, rv = EC_SUCCESS;
	int len = 0;

	/* Fetch the header */
	rv |= tcpc_read16(port, TCPC_REG_PD_HEADER, &reg);
	if (rv) {
		*head = 0;
		/* Clear receive message interrupt bit(bit-0) */
		tcpc_read(port, TCPC_REG_IRQ_SOURCE_RECV_MSG, &reg);
		tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG,
			 reg & (~0x01));

		return EC_ERROR_UNKNOWN;
	}
	*head = reg;
	len = PD_HEADER_CNT(*head) * 4;
	if (!len) {
		/* Clear receive message interrupt bit(bit-0) */
		tcpc_read(port, TCPC_REG_IRQ_SOURCE_RECV_MSG, &reg);
		tcpc_write(port, TCPC_REG_IRQ_SOURCE_RECV_MSG,
			 reg & (~0x01));
		return EC_SUCCESS;
	}

	/* Receive message : assuming payload have enough
	 * memory allocated
	 */
	rv |= anx74xx_read_pd_obj(port, (uint8_t *)payload, len);
	if (rv) {
		*head = 0;
		return EC_ERROR_UNKNOWN;
	}

	return rv;
}

int tcpm_transmit(int port, enum tcpm_transmit_type type,
		  uint16_t header,
		  const uint32_t *data)
{
	unsigned char len = 0;
	int ret = 0, reg = 0;

	switch (type) {
		/* ANX is aware of type */
		case TCPC_TX_SOP:
		case TCPC_TX_SOP_PRIME:
		case TCPC_TX_SOP_PRIME_PRIME:
			len = PD_HEADER_CNT(header) * 4 + 2;
			ret = anx74xx_send_message(port, header,
						   data, type, len);
			break;
		case TCPC_TX_HARD_RESET:
			/* Request HARD RESET */
			tcpc_read(port, TCPC_REG_TX_CTRL_1, &reg);
			reg |= TCPC_REG_TX_HARD_RESET_REQ;
			ret = tcpc_write(port, TCPC_REG_TX_CTRL_1, reg);
			break;
		case TCPC_TX_CABLE_RESET:
			/* Request CABLE RESET */
			tcpc_read(port, TCPC_REG_TX_CTRL_1, &reg);
			reg |= TCPC_REG_TX_CABLE_RESET_REQ;
			ret = tcpc_write(port, TCPC_REG_TX_CTRL_1, reg);
			break;
		default:
			return EC_ERROR_UNIMPLEMENTED;
	}

	return ret;
}

void tcpc_alert(int port)
{
	int status;

	/* Check the alert status */
	if (anx74xx_alert_status(port, &status))
		status = 0;
	if (status) {

		if (status & TCPC_REG_ALERT_CC_CHANGE) {
			/* CC status changed, wake task */
			task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
		}

		/* If alert is to receive a message */
		if (status & TCPC_REG_ALERT_MSG_RECV) {
			/* Set a PD_EVENT_RX */
			task_set_event(PD_PORT_TO_TASK_ID(port),
				       PD_EVENT_RX, 0);
		}
		if (status & TCPC_REG_ALERT_TX_ACK_RECV) {
			/* Inform PD about this TX success */
			pd_transmit_complete(port,
					     TCPC_TX_COMPLETE_SUCCESS);
		}
		if (status & TCPC_REG_ALERT_TX_MSG_ERROR) {
			/* let PD does not wait for this */
			pd_transmit_complete(port,
					     TCPC_TX_COMPLETE_FAILED);
		}
		if (status & TCPC_REG_ALERT_TX_CABLE_RESETOK) {
			/* ANX hardware clears the request bit */
			pd_transmit_complete(port,
					     TCPC_TX_COMPLETE_SUCCESS);
		}
		if (status & TCPC_REG_ALERT_TX_HARD_RESETOK) {
			/* ANX hardware clears the request bit */
			pd_transmit_complete(port,
					     TCPC_TX_COMPLETE_SUCCESS);
		}
	}
}

int tcpm_init(int port)
{
	int rv = 0, reg;

	memset(anx, 0, CONFIG_USB_PD_PORT_COUNT*sizeof(struct anx_state));
	/* Bring chip in normal mode to work */
	anx74xx_set_power_mode(port, TCPC_NORMAL_MODE);

	/* Initialize analog section of ANX */
	rv |= anx74xx_init_analog(port);

	/* interrupt polarity to low active */
	rv |= tcpc_write(port, TCPC_REG_IRQ_STATUS, TCPC_REG_IRQ_POL_LOW);

	/* disable all interrupts */
	rv |= tcpc_write(port, TCPC_REG_IRQ_EXT_MASK_0,
			 TCPC_REG_CLEAR_SET_BITS);

	/* unmask interrupts */
	rv |= tcpc_read(port, TCPC_REG_IRQ_EXT_MASK_0, &reg);
	reg &= (~TCPC_REG_ALERT_TX_MSG_ERROR);
	reg &= (~TCPC_REG_ALERT_TX_CABLE_RESETOK);
	reg &= (~TCPC_REG_ALERT_TX_HARD_RESETOK);
	rv |= tcpc_write(port, TCPC_REG_IRQ_EXT_MASK_0, reg);

	/*  HPD pin output enable*/
	rv |= tcpc_write(port, TCPC_REG_HPD_CTRL_0, TCPC_REG_HPD_DEFAULT);

	if (rv)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}
