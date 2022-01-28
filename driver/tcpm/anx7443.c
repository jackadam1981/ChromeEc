/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ANX7443 retimer control */

#include "anx7443.h"
#include "anx7406.h"

static int anx7443_mux_write(const struct usb_mux *me, int slave,
			     int offset, int data)
{
	int port = me->usb_port;

#ifdef CONFIG_USB_PD_MUX_CTRL_BY_ANX7406
	return anx7406_m0_write(port, slave, offset, data);
#else
	return i2c_write8(tcpc_config[port].i2c_info.port,
			  I2C_ADDR(slave), offset, data);
#endif
}

int anx7443_mux_read(const struct usb_mux *me, int slave, int offset)
{
	int port = me->usb_port;

#ifdef CONFIG_USB_PD_MUX_CTRL_BY_ANX7406
	return anx7406_m0_read(port, slave, offset);
#else
	int rv, val;

	rv = i2c_read8(tcpc_config[port].i2c_info.port,
		       I2C_ADDR(slave), offset, &val);
	if (rv) {
		ccprintf("read anx7443 register failed at %x:%x!\n",
			 slave, offset);
		return rv;
	}

	return val;
#endif
}

static int anx7443_set_retimer(const struct usb_mux *me, int flip, int mux_type)
{
	int rv, val;

#if defined(CONFIG_USB_PD_TCPM_ANX7406) && defined(CONFIG_USB_PD_TCPM_ANX7406_AUX)
	rv = anx7406_set_aux(me->usb_port, flip);
	if (rv) {
		ccprintf("Configure AUX failed\n");
		return rv;
	}
#endif

	ccprintf("Set retimer %s flip\n", flip ? "" : "not");

	rv = anx7443_mux_write(me, I2C0_TOP_SLAVE, INSERT_CR_PATTERN_0, 0xAA);
	if (rv) {
		ccprintf("Write INSERT_CR_PATTERN_0 register failed\n");
		return rv;
	}

	rv = anx7443_mux_write(me, I2C0_TOP_SLAVE, DP_RX_REG0,
			      HIGHEST_DRV_STRENTH | RX_GAIN |
			      LFE_EN | CTLE_CTRL);
	if (rv) {
		ccprintf("Write DP_RX_REG0 register failed\n");
		return rv;
	}

	rv = anx7443_mux_write(me, I2C0_TOP_SLAVE, DP_RX_REG3,
			      CTLE_DRV_STRENTH | CTLE_OFF_CANCELLING_EN);
	if (rv) {
		ccprintf("Write DP_RX_REG3 register failed\n");
		return rv;
	}

	rv = anx7443_mux_write(me, I2C0_DP_SLAVE, POWER_DOWN, 0x00);
	if (rv) {
		ccprintf("Write POWER_DOWN register failed\n");
		return rv;
	}

	rv = anx7443_mux_write(me, I2C0_USB_SLAVE, FLIP_CTRL, USB_AUX_FLIP_EN);
	if (rv) {
		ccprintf("Write FLIP_CTRL register failed\n");
		return rv;
	}

	val = CONFIG_REG_EN;
	if (flip)
		val |= FLIP_EN;

	switch (mux_type) {
	case USB_PD_MUX_DOCK:
		val |= DP_EN | USB_EN;
		break;
	case USB_PD_MUX_DP_ENABLED:
		val |= DP_EN;
		break;
	default:
		val |= USB_EN;
		break;
	}

	rv = anx7443_mux_write(me, I2C0_TOP_SLAVE, CONFIG_MODE, val);
	if (rv) {
		ccprintf("Write CONFIG_MODE register failed\n");
		return rv;
	}
	//rv = anx7443_mux_read(me, I2C0_TOP_SLAVE, CONFIG_MODE);
	//ccprintf("Configured CONFIG_MODE = 0x%x\n", rv);

	return 0;
}

static int anx7443_mux_state[CONFIG_USB_PD_PORT_MAX_COUNT];
static int anx7443_mux_set(const struct usb_mux *me, mux_state_t mux_state,
			   bool *ack_required)
{
	int cc_direction = mux_state & USB_PD_MUX_POLARITY_INVERTED;
	mux_state_t mux_type = mux_state & USB_PD_MUX_DOCK;
	int port = me->usb_port;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	ccprintf("C%d mux_state = 0x%x, mux_type = 0x%x\n",
		 port, mux_state, mux_type);

	return anx7443_set_retimer(me, cc_direction, mux_type);
}

static int anx7443_mux_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int port = me->usb_port;

	*mux_state = anx7443_mux_state[port];

	return EC_SUCCESS;
}

static int anx7443_mux_init(const struct usb_mux *me)
{
	int port = me->usb_port;
	bool unused;

	anx7443_mux_state[port] = 0;

	/*
	 * ANX initializes its muxes to (USB_PD_MUX_USB_ENABLED |
	 * USB_PD_MUX_DP_ENABLED) when reinitialized, we need to force
	 * initialize it to USB_PD_MUX_NONE
	 */
	return anx7443_mux_set(me, USB_PD_MUX_NONE, &unused);
}


const struct usb_mux_driver anx7443_usbc_retimer_driver = {
	.init = anx7443_mux_init,
	.set  = anx7443_mux_set,
	.get  = anx7443_mux_get
};
