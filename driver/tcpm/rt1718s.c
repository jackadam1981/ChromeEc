/* Copyright 2021 The Richtek Technology Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * RT1718S TCPC Driver
 */

#include "console.h"
#include "driver/tcpm/rt1718s.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "stdint.h"
#include "task.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#define ENABLE_FAST_ROLE_SWAP		0

/* i2c_write function which won't wake TCPC from low power mode. */
static int rt1718s_write8(int port, int reg, int val)
{
	if (reg & 0xF200) {
		uint8_t out[3];

		out[0] = (reg & 0xFF00) >> 8;
		out[1] = (reg & 0x00FF);
		out[2] = val;

		return i2c_xfer(tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			out, sizeof(out), NULL, 0);
	} else {
		return i2c_write8(tcpc_config[port].i2c_info.port,
			  tcpc_config[port].i2c_info.addr_flags, reg, val);
	}
}

static int rt1718s_read8(int port, int reg, int *val)
{
	if (reg & 0xF200) {
		int rv;
		uint8_t out[2], in;

		out[0] = (reg & 0xFF00) >> 8;
		out[1] = (reg & 0x00FF);

		rv = i2c_xfer(tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags,
			out, sizeof(out), &in, sizeof(in));
		*val = (int)in;

		return rv;
	} else {
		return i2c_read8(tcpc_config[port].i2c_info.port,
			  tcpc_config[port].i2c_info.addr_flags, reg, val);
	}
}

static int rt1718s_update_bits8(int port, int reg, int mask, int val)
{
	int reg_val;

	if (mask == 0xFF)
		return rt1718s_write8(port, reg, val);

	RETURN_ERROR(rt1718s_read8(port, reg, &reg_val));

	reg_val &= (~mask);
	reg_val |= (mask & val);
	return rt1718s_write8(port, reg, reg_val);
}

static int rt1718s_sw_reset(int port)
{
	int rv;

	rv = rt1718s_update_bits8(port, RT1718S_SYS_CTRL3,
		RT1718S_SWRESET_MASK,RT1718S_SWRESET_MASK);

	msleep(2);

	return rv;
}

static int rt1718s_init(int port)
{
	int sys_ctrl1;

	RETURN_ERROR(rt1718s_read8(port, RT1718S_SYS_CTRL1, &sys_ctrl1));

	if (!(sys_ctrl1 & 0x20))
		RETURN_ERROR(rt1718s_sw_reset(port));

	/* set GPIO2 is push pull, as output, output low. */
	RETURN_ERROR(rt1718s_update_bits8(port, RT1718S_GPIO2_CTRL, 0x0E, 0x0C));
	RETURN_ERROR(rt1718s_update_bits8(port, RT1718S_GPIO1_CTRL, 0x0E, 0x0C));

	/* set GPB floating when disable sink vbus by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xDE, 0xF0, 0x80));

	/* set GPB low when enable sink vbus by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xDF, 0xF0, 0x20));

	/* set GPB floating when disable source vbus by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xE0, 0xF0, 0x80));

	/* set GPB floating when enable source vbus by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xE1, 0xF0, 0x80));

	/* set GPB floating when enable source HV vbus by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xE2, 0xF0, 0x80));

#if ENABLE_FAST_ROLE_SWAP
	/* set GPB floating when RT1718S transmits frs signal (sink path off). */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xE8, 0xF0, 0x80));

	/* set GPB floating when frs signal received (sink path off). */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xE9, 0xF0, 0x80));
#endif

	/* set GPB (sink path) being controlled by 0x23. GPA not being controlled by 0x23. */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xEC, 0x30, 0x20));

#if ENABLE_FAST_ROLE_SWAP

	/* set GPB frs action after vbus<5.5V */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xCE, 0x02, 0x02));

	/* set vbus frs low unmasked, Rx frs unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x91, 0xC0, 0xC0));
#endif
	/* set vendor defince alert unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x13, 0x80, 0x80));

	/* set Vconn ovp cc1/2 interrupt unmasked */
	RETURN_ERROR(rt1718s_update_bits8(port, 0x92, 0x03, 0x03));

	/* Disable FOD function */
	RETURN_ERROR(rt1718s_update_bits8(port, 0xCF, 0x40, 0x00));

	/* tcpc connect invalid disabled. Exit shipping mode */
	RETURN_ERROR(rt1718s_update_bits8(port, RT1718S_SYS_CTRL1, 0x60, 0x20));

	RETURN_ERROR(rt1718s_write8(port, 0x1F, 0xFF));

	RETURN_ERROR(rt1718s_write8(port, 0x11, 0xFF));

	tcpci_tcpm_init(port);

	RETURN_ERROR(rt1718s_update_bits8(port, 0x13, 0x80, 0x80));

	return 0;
}

static void rt1718s_alert(int port)
{
	int alert_h;

	rt1718s_read8(port, 0x11, &alert_h);
	if (alert_h & 0x80) {
		CPRINTS("\x1b[1;31mALARM_VBUS_VOLTAGE_L!!\x1b[m");
		rt1718s_write8(port, 0x99, 0xFF);

		rt1718s_write8(port, 0x11, 0x80);
	}

	tcpci_tcpc_alert(port);
}

/* RT1718S is a TCPCI compatible port controller */
const struct tcpm_drv rt1718s_tcpm_drv = {
	.init			= &rt1718s_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
#ifdef CONFIG_USB_PD_DECODE_SOP
	.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
#endif
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &rt1718s_alert,
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus	= &tcpci_tcpc_discharge_vbus,
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
	.get_chip_info		= &tcpci_get_chip_info,
#ifdef CONFIG_USB_PD_PPC
	.set_snk_ctrl		= &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl		= &tcpci_tcpm_set_src_ctrl,
#endif
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &tcpci_enter_low_power_mode,
#endif
};

int command_rt_dump(int argc, char **argv)
{
	for (int i = 0; i <= 0xEF; i++) {
		int val = 0;
		int rt = rt1718s_read8(1, i, &val);

		if (i % 16 == 0)
			CPRINTF("%02X: ", i);
		if (rt)
			CPRINTF("-- ");
		else
			CPRINTF("%02X ", val);
		if (i % 16 == 15)
			CPRINTF("\n");
	}
	return 0;
}
DECLARE_CONSOLE_COMMAND(rt_dump, command_rt_dump, "", "");
