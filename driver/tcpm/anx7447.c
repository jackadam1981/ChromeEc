/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ANX7447 port manager */

#include "hooks.h"
#include "tcpci.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "console.h"
#include "usb_pd.h"
#include "anx7447.h"
#include "ec_version.h"
#include "util.h"

#define ANX7447_VENDOR_ALERT    (1 << 15)

#define ANX7447_REG_STATUS      0x82
#define ANX7447_REG_STATUS_LINK (1 << 0)

#define ANX7447_REG_POWER_STATUS 0x1E
#define ANX7447_REG_TCPC_INITIALIZATION_STATUS (1 << 6)

#define ANX7447_REG_HPD         0x83
#define ANX7447_REG_HPD_HIGH    (1 << 0)
#define ANX7447_REG_HPD_IRQ     (1 << 1)
#define ANX7447_REG_HPD_ENABLE  (1 << 2)

#define ANX7447_USBC_ADDR		0x52
#define ANX7447_REG_RAMCTRL		0xe7
#define ANX7447_REG_RAMCTRL_BOOT_DONE	(1 << 6)

#define INTR_ALERT_MASK_0 0xC9

#define vsafe5v_min (3800/25)
#define vsafe0v_max (800/25)
#define is_equal_greater_safe5v() (((anx7447_get_vbus_voltage())) > vsafe5v_min)
#define is_equal_greater_safe0v() (((anx7447_get_vbus_voltage())) > vsafe0v_max)

#define HPD_HIGH 1
#define HPD_LOW 0

const uint8_t ANX7447_TCPM_VERSION = 0x03;

struct anx_state {
	int	mux_state;
};

static struct anx_state anx[CONFIG_USB_PD_PORT_COUNT];

void anx7447_hpd_mode_en(void)
{
	int reg = 0;

	i2c_read8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		  ANX7447_REG_HPD_CTRL_0, &reg);
	reg |= ANX7447_REG_HPD_MODE;
	i2c_write8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		   ANX7447_REG_HPD_CTRL_0, reg);
}

void anx7447_hpd_output_en(void)
{
	int reg = 0;

	i2c_read8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		  ANX7447_REG_HPD_DEGLITCH_H, &reg);
	reg |= ANX7447_REG_HPD_OEN;
	i2c_write8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		   ANX7447_REG_HPD_DEGLITCH_H, reg);
}

void anx7447_set_hpd_level(int hpd_lvl)
{
	int reg = 0;

	i2c_read8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		  ANX7447_REG_HPD_CTRL_0, &reg);
	if (hpd_lvl)
		reg |= ANX7447_REG_HPD_OUT;
	else
		reg &= ~ANX7447_REG_HPD_OUT;
	i2c_write8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
		   ANX7447_REG_HPD_CTRL_0, reg);
}

static int anx7447_init(int port)
{
	int rv = 0;
	int mask = 0;
	int reg = 0;

	ccprintf("\n======================================================\n");
	ccprintf("ANX7447 TCPM Driver version: v%x.%x\n",
		  ((ANX7447_TCPM_VERSION >> 4) & 0x0F),
		  (ANX7447_TCPM_VERSION & 0x0F));
	ccprintf("Build at: %s\n", DATE);
	ccprintf("======================================================\n\n");

	ccprintf("anx7447_init\n");
	memset(&anx[port], 0, sizeof(struct anx_state));
	/*
	 * 7688 POWER_STATUS[6] is not reliable for tcpci_tcpm_init() to poll
	 * due to it is default 0 in HW, and we cannot write TCPC until it is
	 * ready, or something goes wrong. (Issue 52772)
	 * Instead we poll TCPC 0x50:0xe7 bit6 here to make sure bootdone is
	 * ready(50ms). Then PD main flow can process cc debounce in 50ms ~
	 * 100ms to follow cts.
	 */
	while (1) {

		rv = i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR,
			       ANX7447_REG_POWER_STATUS, &mask);

			/* clear ALERT mask */
		i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR,
			   INTR_ALERT_MASK_0, 0x00);
		i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0x9e, 0x80);

		/* open sop det */
		i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0x2f, 0x21);

		ccprintf("looply waiting for EEPROM done %x, result %x\n",
			  mask, rv);

		tcpc_write(port, 0x9e, 0x80);

		if (rv == EC_SUCCESS &&
		    !(mask & ANX7447_REG_TCPC_INITIALIZATION_STATUS)) {
			ccprintf("exit anx_init()\n");

			rv = i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR,
			       0x9e, &mask);

			ccprintf("0x9e %x\n", mask);

			break;
		}

		/*
		rv = i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR,
			       0x9e, &mask);

			ccprintf("0x9e %x\n", mask);
		*/

		msleep(10);
	}

	/* clear ALERT mask */
	i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, INTR_ALERT_MASK_0, 0x00);
	i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0x9e, 0x80);
	/*
	//ocm reset
	i2c_write8(I2C_PORT_TCPC, 0x7e, 0x88, 0x40);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0x10, 0xff);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0x11, 0xff);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0x12, 0x0c);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0x13, 0x00);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0x2f, 0x01);
	i2c_write8(I2C_PORT_TCPC, 0x58, 0xbf, 0x00);
	*/

	/* reset */
	i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0xcd, &reg);
	reg &= ~(0x1 << 5);
	i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0xcd, reg);

	i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0xbf, &reg);
	reg |= (0x1 << 5);
	i2c_write8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0xbf, reg);

	ccprintf("reset ANX7447 chip\n");
	rv = tcpci_tcpm_drv.init(port);

	anx7447_hpd_mode_en();
	anx7447_set_hpd_level(HPD_LOW);
	anx7447_hpd_output_en();

	if (rv)
		return rv;


	/* enable vendor specific alert */
	/*
	rv = tcpc_write16(port, TCPC_REG_ALERT_MASK, 0x05);
	*/
	return rv;
}

static void anx7447_update_hpd_enable(int port)
{
	int status, reg, rv;

	rv = tcpc_read(port, ANX7447_REG_STATUS, &status);
	rv |= tcpc_read(port, ANX7447_REG_HPD, &reg);
	if (rv)
		return;

	if (!(reg & ANX7447_REG_HPD_ENABLE) ||
	    !(status & ANX7447_REG_STATUS_LINK)) {
		reg &= ~ANX7447_REG_HPD_IRQ;
		tcpc_write(port, ANX7447_REG_HPD,
			   (status & ANX7447_REG_STATUS_LINK)
			   ? reg | ANX7447_REG_HPD_ENABLE
			   : reg & ~ANX7447_REG_HPD_ENABLE);
	}
}

int anx7447_hpd_disable(int port)
{
	return tcpc_write(port, ANX7447_REG_HPD, 0);
}

int anx7447_update_hpd(int port, int level, int irq)
{
	int reg, rv;

	rv = tcpc_read(port, ANX7447_REG_HPD, &reg);
	if (rv)
		return rv;

	if (level)
		reg |= ANX7447_REG_HPD_HIGH;
	else
		reg &= ~ANX7447_REG_HPD_HIGH;

	if (irq)
		reg |= ANX7447_REG_HPD_IRQ;
	else
		reg &= ~ANX7447_REG_HPD_IRQ;

	return tcpc_write(port, ANX7447_REG_HPD, reg);
}

int anx7447_enable_cable_detection(int port)
{
	return tcpc_write(port, TCPC_REG_COMMAND, 0xff);
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
static int anx7447_get_vbus_voltage(void)
{
	int vbus_volt = 0;

	i2c_read16(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0x70, &vbus_volt);
	/*
	ccprintf("reg 0x70~0x71 = 0x%x\n", vbus_volt);
	*/

	return vbus_volt;
}

static int anx7447_tcpm_get_vbus_level(int port)
{
	return is_equal_greater_safe5v();
}
#endif

int anx7447_set_power_supply_ready(int port)
{
	int count = 0;

	while (is_equal_greater_safe0v()) {
		if (count >= 10)
			break;
		msleep(100);
		count++;
	}

	return tcpc_write(port, TCPC_REG_COMMAND, 0x77);
}

int anx7447_power_supply_reset(int port)
{
	return tcpc_write(port, TCPC_REG_COMMAND, 0x66);
}

int anx7447_board_charging_enable(int port, int enable)
{
	int reg = 0;

	if (enable)
		reg = 0x55;
	else
		reg = 0x44;

	return tcpc_write(port, TCPC_REG_COMMAND, reg);
}

static void anx7447_tcpc_alert(int port)
{
	int alert, rv;

	rv = tcpc_read16(port, TCPC_REG_ALERT, &alert);
	/* process and clear alert status */
	tcpci_tcpm_drv.tcpc_alert(port);

	if (!rv && (alert & ANX7447_VENDOR_ALERT))
		anx7447_update_hpd_enable(port);
}

/*
 * timestamp of the next possible toggle to ensure the 2-ms spacing
 * between IRQ_HPD.
 */
static uint64_t hpd_deadline[CONFIG_USB_PD_PORT_COUNT];

void anx7447_tcpc_update_hpd_status(int port, int hpd_lvl, int hpd_irq)
{
	int reg = 0;

	anx7447_set_hpd_level(hpd_lvl);

	if (hpd_irq) {
		uint64_t now = get_time().val;
		/* wait for the minimum spacing between IRQ_HPD if needed */
		if (now < hpd_deadline[port])
			usleep(hpd_deadline[port] - now);

		i2c_read8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
			  ANX7447_REG_HPD_CTRL_0, &reg);
		reg &= ~ANX7447_REG_HPD_OUT;
		i2c_write8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
			   ANX7447_REG_HPD_CTRL_0, reg);
		usleep(HPD_DSTREAM_DEBOUNCE_IRQ);
		reg |= ANX7447_REG_HPD_OUT;
		i2c_write8(I2C_PORT_TCPC, ANX7447_SPI_ADDR,
			   ANX7447_REG_HPD_CTRL_0, reg);
	}
	/* enforce 2-ms delay between HPD pulses */
	hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;
}

void anx7447_tcpc_clear_hpd_status(int port)
{
	anx7447_hpd_output_en();
	anx7447_set_hpd_level(HPD_LOW);
}

#ifdef CONFIG_USB_PD_TCPM_MUX
static int anx7447_mux_init(int i2c_addr)
{
	int port = i2c_addr;

	/* Nothing to do here, ANX initializes its muxes
	 * as (MUX_USB_ENABLED | MUX_DP_ENABLED)
	 */
	anx[port].mux_state = MUX_USB_ENABLED | MUX_DP_ENABLED;

	return EC_SUCCESS;
}

static int anx7447_mux_set(int i2c_addr, mux_state_t mux_state)
{
	int cc_direction;
	mux_state_t mux_type;
	int port = i2c_addr;

	cc_direction = mux_state & MUX_POLARITY_INVERTED;
	mux_type = mux_state & TYPEC_MUX_DOCK;
	ccprintf("mux_state = 0x%x, cc_direction = %d, mux_type = 0x%x\n",
		  mux_state, cc_direction, mux_type);

	/* type-C interface detect calbe plug direction
	 * is postitive orientation
	 */
	/* CC1_CONNECTED */
	if (cc_direction == 0) {
		/* cc1 connection */
		if (mux_type == TYPEC_MUX_DOCK) {
			/* L0-a10/11,L1-b2/b3, sstx-a2/a3, ssrx-b10/11 */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x21);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x21);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x03);
		} else if (mux_type == TYPEC_MUX_DP) {
			/* L0-a10/11,L1-b2/b3, L2-a2/a3, L3-b10/11 */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x09);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x09);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x03);
		} else if (mux_type == TYPEC_MUX_USB) {
			/* Added for USB3.1 only */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x30);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x30);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x00);
		}
	} else {
		/* cc2 connection */
		if (mux_type == TYPEC_MUX_DOCK) {
			/* L0-b10/11,L1-a2/b3, sstx-b2/a3, ssrx-a10/11 */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x12);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x12);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x0C);
		} else if (mux_type == TYPEC_MUX_DP) {
			/* L0-b10/11,L1-a2/b3, L2-b2/a3, L3-a10/11 */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x06);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x06);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x0C);
		} else if (mux_type == TYPEC_MUX_USB) {
			/* Added for USB3.1 only */
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_0, 0x30);
			tcpc_write(port, ANX7447_REG_TCPC_SWITCH_1, 0x30);
			tcpc_write(port, ANX7447_REG_TCPC_AUX_SWITCH, 0x00);
		}
	}

	anx[port].mux_state = mux_state;

	return EC_SUCCESS;
}

/* current mux state */
static int anx7447_mux_get(int i2c_addr, mux_state_t *mux_state)
{
	int port = i2c_addr;

	*mux_state = anx[port].mux_state;

	return EC_SUCCESS;
}
#endif

static int anx7447_tcpm_set_polarity(int port, int polarity)
{
	int res = 0, val = 0;
	i2c_read8(I2C_PORT_TCPC, ANX7447_USBC_ADDR, 0x19, &val);

	while (val != polarity) {
		res = i2c_write8(port, ANX7447_USBC_ADDR, 0x19, polarity);
		if (res)
			return res;

		res = i2c_read8(port, ANX7447_USBC_ADDR, 0x19, &val);

		if (res)
			return res;
	}

	return res;
}

/* ANX7447 is a TCPCI compatible port controller */
const struct tcpm_drv anx7447_tcpm_drv = {
	.init			= &anx7447_init,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.get_vbus_level		= &anx7447_tcpm_get_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &anx7447_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message		= &tcpci_tcpm_get_message,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &anx7447_tcpc_alert,
};

#ifdef CONFIG_USB_PD_TCPM_MUX
const struct usb_mux_driver anx7447_usb_mux_driver = {
	.init = anx7447_mux_init,
	.set = anx7447_mux_set,
	.get = anx7447_mux_get,
};
#endif /* CONFIG_USB_PD_TCPM_MUX */

