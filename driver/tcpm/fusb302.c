/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Author: Gabe Noblesmith
 */

/* Type-C port manager for Fairchild's FUSB302 */

#include "i2c.h"
#include "task.h"
#include "fusb302.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_tcpc.h"
#include "usb_pd_tcpm.h"
#include "util.h"
#include "console.h"

/* Convert port number to tcpc i2c address */
#define I2C_ADDR_TCPC(p) (CONFIG_TCPC_I2C_BASE_ADDR + 2*(p))

/* TODO: copy these local variables for each port */
/*			(assuming only one port today) */
static int cc_polarity;
static int vconn_enabled;
/* pulling_up indicates UFP vs DFP... */
/* 1 = pulling up (DFP) 0 = pulling down (UFP) */
static int pulling_up;
static int rx_enable;
static int dfp_toggling_on;

static int togdone_pullup_cc1;
static int togdone_pullup_cc2;

static int tx_hard_reset_req;

/* bring the FUSB302 out of reset after Hard Reset signaling */
static void fusb302_pd_reset(int port)
{
	i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_RESET, TCPC_REG_RESET_PD_RESET);
}

static void fusb302_flush_rx_fifo(int port)
{
	int reg;

	/* other bits in the register _should_ be 0 */
	/* until the day we support other SOP* types... */
	/* then we'll have to keep a shadow of what this register
	 * value should be so we don't clobber it here! */
	reg = TCPC_REG_CONTROL1_RX_FLUSH;
	i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_CONTROL1, reg);
}

static void fusb302_flush_tx_fifo(int port)
{
	int reg;
	i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_CONTROL0, &reg);
	reg |= TCPC_REG_CONTROL0_TX_FLUSH;
	i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_CONTROL0, reg);
}

static void fusb302_auto_goodcrc_enable(int port, int enable)
{
	int reg;
	i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES1, &reg);

	if (enable)
		reg |= TCPC_REG_SWITCHES1_AUTO_GCRC;
	else
		reg &= ~TCPC_REG_SWITCHES1_AUTO_GCRC;

	i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES1, reg);
}

/* Convert BC LVL values (in FUSB302) to Type-C CC Voltage Status */
static int convert_bc_lvl(int bc_lvl)
{
	/* assume OPEN unless one of the following conditions is true... */
	int ret = TYPEC_CC_VOLT_OPEN;

	if (pulling_up) {
		if (bc_lvl == 0x00)
			ret = TYPEC_CC_VOLT_RA;
		else if (bc_lvl < 0x3)
			ret = TYPEC_CC_VOLT_RD;
	} else {
		if (!pulling_up && (bc_lvl > 0x00))
			ret = TYPEC_CC_VOLT_RD;
	}

	return ret;
}

/* Parse header bytes for the size of packet */
static int get_num_bytes(uint16_t header)
{
	int rv;

	/* Grab the Number of Data Objects field.*/
	rv = (header >> 12) & 0x7;

	/* Multiply by four to go from 32-bit words -> bytes */
	rv *= 4;

	/* Plus 2 for header */
	rv += 2;

	return rv;
}

static int fusb302_send_message(int port, uint16_t header, const uint32_t *data,
				 uint8_t *buf, int buf_pos)
{
	int rv = 0;
	int reg;
	int len;
	int i;
	int byte_count;
	int word_count;

	len = get_num_bytes(header);

	/* packsym tells the TXFIFO that the next X bytes are payload,
	 * and should not be interpreted as special tokens.
	 * The 5 LSBs represent X, the number of bytes. */
	reg = FUSB302_TKN_PACKSYM;
	reg |= (len & 0x1F);

	buf[buf_pos++] = reg;

	/* write in the header */
	reg = header;
	buf[buf_pos++] = reg & 0xFF;

	reg >>= 8;
	buf[buf_pos++] = reg & 0xFF;

	/* header is done, subtract from length to make this for-loop simpler */
	len -= 2;

	/* write data objects, if present */
	byte_count = 0;
	word_count = 0;
	for (i = 0; i < len; i++) {
		reg = data[word_count] >> (8 * byte_count);

		buf[buf_pos++] = reg & 0xFF;

		byte_count++;
		if (byte_count >= 4) {
			byte_count = 0;
			word_count++;
		}
	}

	/* put in the CRC */
	reg = FUSB302_TKN_JAMCRC;
	buf[buf_pos++] = reg & 0xFF;

	/* put in EOP */
	reg = FUSB302_TKN_EOP;
	buf[buf_pos++] = reg & 0xFF;

	/* Turn transmitter off after sending message */
	reg = FUSB302_TKN_TXOFF;
	buf[buf_pos++] = reg & 0xFF;

	/* Start transmission */
	reg = FUSB302_TKN_TXON;
	buf[buf_pos++] = reg & 0xFF;

	/* burst write for speed! */
	i2c_lock(I2C_PORT_TCPC, 1);
	i2c_xfer(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
		buf, buf_pos, 0, 0, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_TCPC, 0);

	return rv;
}

int tcpm_init(int port)
{
	uint8_t rv = 1;
	int reg;

	 /* set default */
	cc_polarity = -1;
	vconn_enabled = 0;
	pulling_up = 0;
	rx_enable = 0;
	dfp_toggling_on = 0;
	togdone_pullup_cc1 = 0;
	togdone_pullup_cc2 = 0;
	tx_hard_reset_req = 0;

	while (rv) {
		rv = 0;

		/* Restore default settings */
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_RESET, TCPC_REG_RESET_SW_RESET);

		/* Turn on retries and set number of retries */
		/* Turn on Auto Soft Reset and Auto Hard Reset */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL3, &reg);
		reg |= TCPC_REG_CONTROL3_AUTO_RETRY;
		reg |= (PD_RETRY_COUNT & 0x3) <<
			TCPC_REG_CONTROL3_N_RETRIES_POS;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL3, reg);

		/* Create interrupt masks */
		reg = 0xFF;
		/* CC level changes */
		reg &= ~TCPC_REG_MASK_BC_LVL;
		/* collisions */
		reg &= ~TCPC_REG_MASK_COLLISION;
		/* misc alert */
		reg &= ~TCPC_REG_MASK_ALERT;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_MASK, reg);

		reg = 0xFF;
		 /* informs of attaches */
		reg &= ~TCPC_REG_MASKA_TOGDONE;
		/* when all pd message retries fail... */
		reg &= ~TCPC_REG_MASKA_RETRYFAIL;
		/* when fusb302 send a hard reset. */
		reg &= ~TCPC_REG_MASKA_HARDSENT;
		/* when fusb302 receives GoodCRC ack for a pd message */
		reg &= ~TCPC_REG_MASKA_TX_SUCCESS;
		/* when fusb302 receives soft reset */
		reg &= ~TCPC_REG_MASKA_SOFTRESET;
		/* when fusb302 receives a hard reset */
		reg &= ~TCPC_REG_MASKA_HARDRESET;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_MASKA, reg);

		reg = 0xFF;
		/* when fusb302 sends GoodCRC to ack a pd message */
		reg &= ~TCPC_REG_MASKB_GCRCSENT;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_MASKB, reg);

		/* Interrupt Enable */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL0, &reg);
		reg &= ~TCPC_REG_CONTROL0_INT_MASK;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL0, reg);

		/* Set VCONN switch defaults */
		tcpm_set_polarity(port, 0);
		tcpm_set_vconn(port, 0);

		/* Turn on the power! */
		/* TODO: Reduce power consumption */
		reg = TCPC_REG_POWER_PWR_ALL;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_POWER, reg);
	}
	return rv;
}

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	int rv;
	int reg;
	int orig_meas_cc1;
	int orig_meas_cc2;
	uint8_t bc_lvl_cc1;
	uint8_t bc_lvl_cc2;

	/* can't measure while doing DFP toggling -
	 * FUSB302 takes control of the switches.
	 * During this time, tell software that CCs are open -
	 * at least until we get the TOGDONE interrupt...
	 * which signals that the hardware found something.
	 */
	if (dfp_toggling_on) {
		*cc1 = TYPEC_CC_VOLT_OPEN;
		*cc2 = TYPEC_CC_VOLT_OPEN;
		return 0;
	}

	/*
	 * Measure CC1 first.
	 */
	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, &reg);

	/* save original state to be returned to later... */
	if (reg & TCPC_REG_SWITCHES0_MEAS_CC1)
		orig_meas_cc1 = 1;
	else
		orig_meas_cc1 = 0;

	if (reg & TCPC_REG_SWITCHES0_MEAS_CC2)
		orig_meas_cc2 = 1;
	else
		orig_meas_cc2 = 0;


	/* Disable CC2 measurement switch, enable CC1 measurement switch */
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC2;
	reg |= TCPC_REG_SWITCHES0_MEAS_CC1;

	rv = i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, reg);

	/*
	 * CC1 is now being measured by FUSB302.
	 */
	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_STATUS0, &reg);
	if (rv)
		return rv;

	reg &= (TCPC_REG_STATUS0_BC_LVL0 |
		TCPC_REG_STATUS0_BC_LVL1);

	/* Save the value for later */
	bc_lvl_cc1 = reg;

	/*
	 * Measure CC2 next.
	 */

	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, &reg);
	if (rv)
		return rv;

	/* Disable CC1 measurement switch, enable CC2 measurement switch */
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC1;
	reg |= TCPC_REG_SWITCHES0_MEAS_CC2;

	rv = i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, reg);
	if (rv)
		return rv;

	/*
	 * CC2 is now being measured by FUSB302.
	 */
	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_STATUS0, &reg);
	if (rv)
		return rv;

	reg &= (TCPC_REG_STATUS0_BC_LVL0 |
		TCPC_REG_STATUS0_BC_LVL1);

	/* Save the value for later */
	bc_lvl_cc2 = reg;

	*cc1 = convert_bc_lvl(bc_lvl_cc1);
	*cc2 = convert_bc_lvl(bc_lvl_cc2);

	/* return MEAS_CC1/2 switches to original state */
	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, &reg);
	if (orig_meas_cc1)
		reg |= TCPC_REG_SWITCHES0_MEAS_CC1;
	else
		reg &= ~TCPC_REG_SWITCHES0_MEAS_CC1;
	if (orig_meas_cc2)
		reg |= TCPC_REG_SWITCHES0_MEAS_CC2;
	else
		reg &= ~TCPC_REG_SWITCHES0_MEAS_CC2;

	rv = i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, reg);

	return rv;
}

int tcpm_set_cc(int port, int pull)
{
	int rv = 0;
	int reg;

	/* NOTE: FUSB302 toggles a single pull-up between CC1 and CC2 */
	/* NOTE: FUSB302 Does not support Ra. */
	switch (pull) {
	case TYPEC_CC_RP:

		/* if fusb302 hasn't figured anything out yet */
		if (!togdone_pullup_cc1 && !togdone_pullup_cc2) {

			/* Enable DFP Toggle Mode */
			rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL2, &reg);

			/* turn on toggle */
			reg |= (TCPC_REG_CONTROL2_MODE_DFP <<
				TCPC_REG_CONTROL2_MODE_POS);
			reg |= TCPC_REG_CONTROL2_TOGGLE;
			rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL2, reg);

			pulling_up = 1;
			dfp_toggling_on = 1;
		} else {

			/* enable the pull-up we know to be necessary */
			rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, &reg);

			reg &= ~(TCPC_REG_SWITCHES0_CC2_PU_EN);
			reg &= ~(TCPC_REG_SWITCHES0_CC1_PU_EN);
			reg &= ~TCPC_REG_SWITCHES0_CC1_PD_EN;
			reg &= ~TCPC_REG_SWITCHES0_CC2_PD_EN;

			if (togdone_pullup_cc1)
				reg |= TCPC_REG_SWITCHES0_CC1_PU_EN;
			else
				reg |= TCPC_REG_SWITCHES0_CC2_PU_EN;

			rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
						TCPC_REG_SWITCHES0, reg);

			pulling_up = 1;
			dfp_toggling_on = 0;
		}

		break;
	case TYPEC_CC_RD:
		/* Enable UFP Mode */

		/* turn off toggle */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL2, &reg);
		reg &= ~TCPC_REG_CONTROL2_TOGGLE;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_CONTROL2, reg);

		/* enable pull-downs, disable pullups */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES0, &reg);

		reg &= ~(TCPC_REG_SWITCHES0_CC2_PU_EN);
		reg &= ~(TCPC_REG_SWITCHES0_CC1_PU_EN);
		reg |= (TCPC_REG_SWITCHES0_CC1_PD_EN);
		reg |= (TCPC_REG_SWITCHES0_CC2_PD_EN);
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES0, reg);

		pulling_up = 0;
		dfp_toggling_on = 0;
		break;
	case TYPEC_CC_OPEN:
		/* Disable toggling */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL2, &reg);
		reg &= ~TCPC_REG_CONTROL2_TOGGLE;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
						TCPC_REG_CONTROL2, reg);

		/* Ensure manual switches are opened */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES0, &reg);
		reg &= ~TCPC_REG_SWITCHES0_CC1_PU_EN;
		reg &= ~TCPC_REG_SWITCHES0_CC2_PU_EN;
		reg &= ~TCPC_REG_SWITCHES0_CC1_PD_EN;
		reg &= ~TCPC_REG_SWITCHES0_CC2_PD_EN;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, reg);

		pulling_up = 0;
		dfp_toggling_on = 0;
		break;
	default:
		/* Unsupported... */
		return 1;
	}
	return rv;
}

int tcpm_set_polarity(int port, int polarity)
{
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */

	int reg;
	uint8_t rv;

	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, &reg);

	/* clear VCONN switch bits */
	reg &= ~TCPC_REG_SWITCHES0_VCONN_CC1;
	reg &= ~TCPC_REG_SWITCHES0_VCONN_CC2;

	if (vconn_enabled) {
		/* set VCONN switch to be non-CC line */
		if (polarity)
			reg |= TCPC_REG_SWITCHES0_VCONN_CC1;
		else
			reg |= TCPC_REG_SWITCHES0_VCONN_CC2;
	}

	/* clear meas_cc bits (RX line select) */
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC1;
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC2;

	/* set rx polarity */
	if (polarity)
		reg |= TCPC_REG_SWITCHES0_MEAS_CC2;
	else
		reg |= TCPC_REG_SWITCHES0_MEAS_CC1;

	rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, reg);

	rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES1, &reg);

	/* clear tx_cc bits */
	reg &= ~TCPC_REG_SWITCHES1_TXCC1_EN;
	reg &= ~TCPC_REG_SWITCHES1_TXCC2_EN;

	/* set tx polarity */
	if (polarity)
		reg |= TCPC_REG_SWITCHES1_TXCC2_EN;
	else
		reg |= TCPC_REG_SWITCHES1_TXCC1_EN;

	rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES1, reg);

	/* Save the polarity for later */
	cc_polarity = polarity;

	return rv;
}

int tcpm_set_vconn(int port, int enable)
{
	/*
	 * FUSB302 does not have dedicated VCONN Enable switch.
	 * We'll get through this by disabling both of the
	 * VCONN - CC* switches to disable, and enabling the
	 * saved polarity when enabling.
	 * Therefore at startup, tcpm_set_polarity should be called first,
	 * or else live with the default put into tcpm_init.
	 */

	uint8_t rv;
	int reg;

	if (enable) {
		/* set to saved polarity */
		rv = tcpm_set_polarity(port, cc_polarity);
	} else {

		rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES0,	&reg);

		/* clear VCONN switch bits */
		reg &= ~TCPC_REG_SWITCHES0_VCONN_CC1;
		reg &= ~TCPC_REG_SWITCHES0_VCONN_CC2;

		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_SWITCHES0, reg);
	}

	/* save enable state for later use */
	vconn_enabled = enable;
	return rv;
}

int tcpm_set_msg_header(int port, int power_role, int data_role)
{
	int reg;
	uint8_t rv;

	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES1, &reg);
	if (rv)
		return rv;

	reg &= ~TCPC_REG_SWITCHES1_POWERROLE;
	reg &= ~TCPC_REG_SWITCHES1_DATAROLE;

	if (power_role)
		reg |= TCPC_REG_SWITCHES1_POWERROLE;
	if (data_role)
		reg |= TCPC_REG_SWITCHES1_DATAROLE;

	rv = i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES1, reg);

	return rv;
}

int tcpm_set_rx_enable(int port, int enable)
{
	int reg;
	int rv;

	rx_enable = enable;

	/* Get current switch state */
	rv = i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			TCPC_REG_SWITCHES0, &reg);

	/* Clear CC1/CC2 measure bits */
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC1;
	reg &= ~TCPC_REG_SWITCHES0_MEAS_CC2;

	if (enable) {
		switch (cc_polarity) {
		/* if CC polarity hasnt been determined, can't enable */
		case -1:
			return 1;
			break;
		case 0:
			reg |= TCPC_REG_SWITCHES0_MEAS_CC1;
			break;
		case 1:
			reg |= TCPC_REG_SWITCHES0_MEAS_CC2;
			break;
		default:
			/* "shouldn't get here" */
			return 1;
			break;
		}
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, reg);

		/* flush rx fifo in case messages have been coming our way */
		fusb302_flush_rx_fifo(port);


	} else {
		/* bit of a hack here.
		 * when this function is called to disable rx (enable=0)
		 * using it as an indication of detach (gulp!)
		 * to reset our knowledge of where
		 * the toggle state machine landed.
		 */
		togdone_pullup_cc1 = 0;
		togdone_pullup_cc2 = 0;

		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, reg);
	}

	fusb302_auto_goodcrc_enable(port, enable);

	return rv;
}

int tcpm_get_message(int port, uint32_t *payload, int *head)
{
	int rv = 0;
	int reg;
	int len;
	int i;
	int byte_count;
	int word_count;
	uint8_t buf[40]; /* TODO: Reduce size and ensure buf doesn't overrun */

	/* NOTE: Assuming enough memory has been allocated for payload. */

	/* PART 1 OF BURST READ: Write in register address. */
	/* Issue a START, no STOP. */
	i2c_lock(I2C_PORT_TCPC, 1);
	buf[0] = TCPC_REG_FIFOS;
	i2c_xfer(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
		buf, 1, 0, 0, I2C_XFER_START);

	/* PART 2 OF BURST READ: Read up to the header. */
	/* Issue a repeated START, no STOP. */
	/* only grab three bytes so we can get the header
	 * and determine how many more bytes we need to read. */
	i2c_xfer(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
		0, 0, buf, 3, I2C_XFER_START);

	/* Grab the header */
	*head = (buf[1] & 0xFF);
	*head |= ((buf[2] << 8) & 0xFF00);

	/* figure out packet length, subtract header bytes */
	len = get_num_bytes(*head) - 2;

	/* PART 3 OF BURST READ: Read everything else. */
	/* No START, but do issue a STOP at the end. */
	/* add 3 to buf address to continue where STEP 2 left off */
	/* add 4 to len to read CRC out */
	i2c_xfer(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
		0, 0, buf, len+4, I2C_XFER_STOP);

	i2c_lock(I2C_PORT_TCPC, 0);

	/* return the data */
	byte_count = 4;
	word_count = 0;
	payload[0] = 0;
	for (i = 0; i < len; i++) {
		reg = buf[i];
		payload[word_count] >>= 8;
		payload[word_count] |= (reg << 24) & 0xFF000000;

		byte_count--;
		if (byte_count <= 0) {
			byte_count = 4;
			word_count++;
			payload[word_count] = 0;
		}
	}

	return rv;
}

int tcpm_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
			 const uint32_t *data)
{
	int rv = 0;
	int buf_pos = 0;
	uint8_t buf[40]; /* TODO: Reduce size and ensure buf doesn't overrun */

	int reg;

	/* Flush the TXFIFO */
	fusb302_flush_tx_fifo(port);

	switch (type) {
	case TCPC_TX_SOP:

		/* put register address first for of burst i2c write */
		buf[buf_pos++] = TCPC_REG_FIFOS;

		/* Write the SOP Ordered Set into TX FIFO */
		buf[buf_pos++] = FUSB302_TKN_SYNC1;
		buf[buf_pos++] = FUSB302_TKN_SYNC1;
		buf[buf_pos++] = FUSB302_TKN_SYNC1;
		buf[buf_pos++] = FUSB302_TKN_SYNC2;

		fusb302_send_message(port, header, data, buf, buf_pos);

		break;
	case TCPC_TX_HARD_RESET:
		tx_hard_reset_req = 1;

		/* Simply hit the SEND_HARD_RESET bit */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL3, &reg);
		reg |= TCPC_REG_CONTROL3_SEND_HARDRESET;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL3, reg);

		break;
	case TCPC_TX_BIST_MODE_2:
		/* Simply hit the BIST_MODE2 bit */
		rv |= i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL1, &reg);
		reg |= TCPC_REG_CONTROL1_BIST_MODE2;
		rv |= i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL1, reg);
		break;
	default:
		rv = 1;
		break;
	}

	return rv;
}

void tcpc_alert(int port)
{
	/* interrupt has been received */
	int interrupt;
	int interrupta;
	int interruptb;
	int reg;
	int toggle_answer;

	/* reading interrupt registers clears them */

	i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_INTERRUPT, &interrupt);
	i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_INTERRUPTA, &interrupta);
	i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_INTERRUPTB, &interruptb);


	if (interrupt & TCPC_REG_INTERRUPT_BC_LVL) {
		/* CC Status change */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
	}

	if (interrupt & TCPC_REG_INTERRUPT_COLLISION) {
		/* packet sending collided */
		tx_hard_reset_req = 0;
		pd_transmit_complete(port, TCPC_TX_COMPLETE_FAILED);
	}

	/*
	 *
	 * if (interrupt & TCPC_REG_INTERRUPT_ALERT) {
	 * miscellaneous alert
	 * }
	 */

	if (interrupta & TCPC_REG_INTERRUPTA_TX_SUCCESS) {
		/* sent packet was acknowledged with a GoodCRC */

		/* flush out the GoodCRC message*/
		fusb302_flush_rx_fifo(port);

		pd_transmit_complete(port, TCPC_TX_COMPLETE_SUCCESS);
	}

	if (interrupta & TCPC_REG_INTERRUPTA_TOGDONE) {
		/* toggle done */
		dfp_toggling_on = 0;

		/* read what 302 settled on for an answer...*/
		i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_STATUS1A, &reg);
		reg = reg >> TCPC_REG_STATUS1A_TOGSS_POS;
		reg = reg & TCPC_REG_STATUS1A_TOGSS_MASK;

		toggle_answer = reg;

		/* Turn off toggle so we can take over the switches again */
		i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL2, &reg);
		reg &= ~TCPC_REG_CONTROL2_TOGGLE;
		i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
				TCPC_REG_CONTROL2, reg);

		switch (toggle_answer) {
		case TCPC_REG_STATUS1A_TOGSS_SRC1:
			togdone_pullup_cc1 = 1;
			togdone_pullup_cc2 = 0;
			break;
		case TCPC_REG_STATUS1A_TOGSS_SRC2:
			togdone_pullup_cc1 = 0;
			togdone_pullup_cc2 = 1;
			break;
		case TCPC_REG_STATUS1A_TOGSS_SNK1:
			togdone_pullup_cc1 = 0;
			togdone_pullup_cc2 = 0;
			break;
		case TCPC_REG_STATUS1A_TOGSS_SNK2:
			togdone_pullup_cc1 = 0;
			togdone_pullup_cc2 = 0;
			break;
		case TCPC_REG_STATUS1A_TOGSS_AA:
			togdone_pullup_cc1 = 0;
			togdone_pullup_cc2 = 0;
			break;
		default:
			/* TODO: should never get here, but? */
			break;
		}

		/* enable the pull-up we know to be necessary */
		i2c_read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, &reg);

		reg &= ~(TCPC_REG_SWITCHES0_CC2_PU_EN);
		reg &= ~(TCPC_REG_SWITCHES0_CC1_PU_EN);
		reg &= ~TCPC_REG_SWITCHES0_CC1_PD_EN;
		reg &= ~TCPC_REG_SWITCHES0_CC2_PD_EN;

		if (togdone_pullup_cc1)
			reg |= TCPC_REG_SWITCHES0_CC1_PU_EN;
		else
			reg |= TCPC_REG_SWITCHES0_CC2_PU_EN;

		i2c_write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
					TCPC_REG_SWITCHES0, reg);
	}
	if (interrupta & TCPC_REG_INTERRUPTA_RETRYFAIL) {
		/* all retries have failed to get a GoodCRC */
		pd_transmit_complete(port, TCPC_TX_COMPLETE_FAILED);
	}
	/*
	 * if (interrupta & TCPC_REG_INTERRUPTA_SOFTFAIL) {
	 * all soft reset retries have failed
	 * }
	 */
	if (interrupta & TCPC_REG_INTERRUPTA_HARDSENT) {
		/* hard reset has been sent */

		if (tx_hard_reset_req) {
			tx_hard_reset_req = 0;
			/* bring FUSB302 out of reset */
			fusb302_pd_reset(port);

			pd_transmit_complete(port, TCPC_TX_COMPLETE_SUCCESS);
		}
	}
	if (interrupta & TCPC_REG_INTERRUPTA_SOFTRESET) {
		/* soft reset has been received */
		/* TODO: Pass it along */
	}
	if (interrupta & TCPC_REG_INTERRUPTA_HARDRESET) {
		/* hard reset has been received */

		/* bring FUSB302 out of reset */
		fusb302_pd_reset(port);

		pd_execute_hard_reset(port);

		task_wake(PD_PORT_TO_TASK_ID(port));
	}


	if (interruptb & TCPC_REG_INTERRUPTB_GCRCSENT) {
		/* Packet received and GoodCRC sent */
		/* (this interrupt fires after the GoodCRC finishes) */
		if (rx_enable) {
			task_set_event(PD_PORT_TO_TASK_ID(port),
					PD_EVENT_RX, 0);
		} else {
			/* flush rx fifo if rx isn't enabled */
			fusb302_flush_rx_fifo(port);
		}
	}


}
