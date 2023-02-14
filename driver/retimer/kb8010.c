/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for Kandou KB801x USB-C 40 Gb/s multiprotocol switch.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "kb8010.h"
#include "retimer/kb8010_public.h"
#include "timer.h"

/* Time between load switch enable and the reset being de-asserted */
#define KB8010_POWER_ON_DELAY_MS 20

static mux_state_t cached_mux_state[CONFIG_USB_PD_PORT_MAX_COUNT];

#define KB8010_LANE_CFG_LEN 8
enum kb8010_modes {
	kb8010_usb3 = KB8010_PROTOCOL_USB3,
	kb8010_dpmf = KB8010_PROTOCOL_DPMF,
	kb8010_dp = KB8010_PROTOCOL_DP,
	kb8010_usb4_tbt = KB8010_PROTOCOL_USB4,
	kb8010_num_modes,
};

struct kb80010_reg_desc {
	uint16_t offset;
	uint8_t val;
};

const static struct kb80010_reg_desc kb8010_init_cfg[] = {
	{ 0xF0A9, 0x03 }, { 0xF0A0, 0x07 }, { 0xFFB9, 0x04 }, { 0xF172, 0x03 },
	{ 0xF173, 0x06 }, { 0xF174, 0x0A }, { 0xF242, 0x13 }, { 0xFECA, 0x01 },
};

const static struct kb80010_reg_desc kb8010_dp_cfg[] = {
	{ 0x0002, 0x06 }, { 0xf2cc, 0x02 }, { 0x6314, 0x09 }, { 0x6393, 0x60 },
	{ 0x603d, 0xba }, { 0x603e, 0x67 }, { 0x603f, 0x91 },
};

const static uint8_t kb8010_flip_cfg[kb8010_num_modes][KB8010_LANE_CFG_LEN] = {
	{ 0x05, 0x02, 0x05, 0x02, 0x02, 0x08, 0x02, 0x08 },
	{ 0x05, 0x20, 0x05, 0x01, 0x13, 0x08, 0x00, 0x08 },
	{ 0x50, 0x20, 0x06, 0x01, 0x13, 0x21, 0x00, 0x00 },
	{ 0x05, 0x02, 0x05, 0x02, 0x02, 0x08, 0x02, 0x08 },
};

static int kb8010_write(const struct usb_mux *me, uint16_t address,
			uint8_t data)
{
	uint8_t kb8010_config[3] = { 0x00, 0x00, 0x00 };

	kb8010_config[0] = (address >> 8) & 0xff;
	kb8010_config[1] = address & 0xff;
	kb8010_config[2] = data;
	return i2c_xfer(me->i2c_port, me->i2c_addr_flags, kb8010_config,
			sizeof(kb8010_config), NULL, 0);
}

static int kb8010_read(const struct usb_mux *me, uint16_t address,
		       uint8_t *data)
{
	uint8_t kb8010_config[2] = { 0x00, 0x00 };

	kb8010_config[0] = (address >> 8) & 0xff;
	kb8010_config[1] = address & 0xff;
	return i2c_xfer(me->i2c_port, me->i2c_addr_flags, kb8010_config,
			sizeof(kb8010_config), data, 1);
}

static int kb8010_pair_write(const struct usb_mux *me,
			     const struct kb80010_reg_desc *pair,
			     const int num_pairs)
{
	int i;
	int rv;

	for (i = 0; i < num_pairs; ++i) {
		rv = kb8010_write(me, pair[i].offset, pair[i].val);
		if (rv != EC_SUCCESS)
			return rv;
	}

	return EC_SUCCESS;
}

static int kb8010_sequential_write(const struct usb_mux *me,
				   const uint16_t start_addr,
				   const uint8_t *values, const uint8_t size)
{
	int i;
	int rv;

	for (i = 0; i < size; ++i) {
		rv = kb8010_write(me, start_addr + i, values[i]);
		if (rv != EC_SUCCESS)
			return rv;
	}

	return EC_SUCCESS;
}

static int kb8010_global_init(const struct usb_mux *me)
{
	return kb8010_pair_write(me, kb8010_init_cfg,
				 ARRAY_SIZE(kb8010_init_cfg));
}

static int kb8010_config_usb4_tbt(const struct usb_mux *me,
				  mux_state_t mux_state)
{
	enum idh_ptype cable_type = 0x0;
	int rv;
	union tbt_mode_resp_cable cable_resp = {
		.raw_value =
			pd_get_tbt_mode_vdo(me->usb_port, TCPCI_MSG_SOP_PRIME)
	};

	cable_type = get_usb_pd_cable_type(me->usb_port);

	/* Special configuration only for legacy mode */
	if (cable_type == IDH_PTYPE_ACABLE ||
	    cable_resp.tbt_active_passive == TBT_CABLE_ACTIVE) {
		/* Active cable */
		if (cable_resp.lsrx_comm == UNIDIR_LSRX_COMM) {
			cable_type |= KB8010_CABLE_TYPE_ACTIVE_UNIDIR;
		} else {
			rv = kb8010_write(me, 0x811f, 0x1d);
			if (rv)
				return rv;
			cable_type |= KB8010_CABLE_TYPE_ACTIVE_BIDIR;
		}
	} else {
		/* Passive Cable */
		cable_type |= KB8010_CABLE_TYPE_PASSIVE;
	}

	return kb8010_write(me, KB8010_REG_ORIENTATION, cable_type);
}

static int kb8010_set_state(const struct usb_mux *me, mux_state_t mux_state,
			    bool *ack_required)
{
	int rv;
	enum kb8010_modes mode = kb8010_usb3;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	cached_mux_state[me->usb_port] = mux_state;
	rv = kb8010_write(me, KB8010_REG_RESET, KB8010_RESET_MASK);
	if (rv)
		return rv;
	/* Release memory map reset */
	rv = kb8010_write(me, KB8010_REG_RESET,
			  KB8010_RESET_MASK & ~KB8010_RESET_MM);
	if (rv)
		return rv;

	/* Already in reset, nothing to do */
	if ((mux_state == USB_PD_MUX_NONE) ||
	    (mux_state & USB_PD_MUX_SAFE_MODE))
		return EC_SUCCESS;

	/* Perform common initialization */
	rv = kb8010_global_init(me);
	if (rv)
		return rv;

	/* USB4/TBT mode */
	if (mux_state &
	    (USB_PD_MUX_USB4_ENABLED | USB_PD_MUX_TBT_COMPAT_ENABLED)) {
		mode = kb8010_usb4_tbt;
		rv = kb8010_config_usb4_tbt(me, mux_state);
		if (rv)
			return rv;
	} else if (mux_state &
		   (USB_PD_MUX_DP_ENABLED | USB_PD_MUX_USB_ENABLED)) {
		/*
		 * USB3, DP, or DPMF mode
		 * DP and DPMF modes require the same set of register writes for
		 * the DP function. USB3 does not require any additional
		 * configuration other than selecting the mode.
		 */
		if (mux_state & USB_PD_MUX_DP_ENABLED) {
			/* DP or DPMF mode utilizes the same DP configuration */
			rv = kb8010_pair_write(me, kb8010_dp_cfg,
					       ARRAY_SIZE(kb8010_dp_cfg));
			if (rv)
				return rv;
			mode = mux_state & USB_PD_MUX_USB_ENABLED ?
				       kb8010_dpmf :
				       kb8010_dp;
		}
	}

	/* Flip configuration */
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		rv = kb8010_write(me, 0x504C, 0x0d);
		if (rv)
			return rv;
		rv = kb8010_write(me, 0x5040, 0x40);
		if (rv)
			return rv;
		rv = kb8010_sequential_write(me, 0x5044, kb8010_flip_cfg[mode],
					     ARRAY_SIZE(kb8010_flip_cfg[mode]));
		if (rv)
			return rv;
	} else {
		rv = kb8010_write(me, 0x504C, 0x02);
		if (rv)
			return rv;
	}

	/* Write mode to protocol register */
	rv = kb8010_write(me, KB8010_REG_PROTOCOL, (uint8_t)mode);
	if (rv)
		return rv;

	/* Enable KB8010 */
	return kb8010_write(me, KB8010_REG_RESET, 0x00);
}

static int kb8010_init(const struct usb_mux *me)
{
	bool unused;

	gpio_set_level(kb8010_controls[me->usb_port].retimer_rst_gpio, 1);

	/*
	 * Delay after enabling power and releasing the reset to allow the power
	 * to come up and the reset to be released by the power sequencing
	 * logic. If after the delay, the reset is still held low - return an
	 * error.
	 */
	msleep(KB8010_POWER_ON_DELAY_MS);
	if (!gpio_get_level(kb8010_controls[me->usb_port].retimer_rst_gpio))
		return EC_ERROR_NOT_POWERED;

	return kb8010_set_state(me, USB_PD_MUX_NONE, &unused);
}

static int kb8010_enter_low_power_mode(const struct usb_mux *me)
{
	/* To endter low power mode, put KB8010 in reset */
	gpio_set_level(kb8010_controls[me->usb_port].retimer_rst_gpio, 0);

	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_RETIMER

static int console_command_kb8010_xfer(int argc, const char **argv)
{
	char rw, *e;
	int rv, port, reg, val;
	uint8_t data;
	const struct usb_mux_chain *mux_chain;
	const struct usb_mux *mux;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;

	/* Get port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || !board_is_usb_pd_port_present(port))
		return EC_ERROR_PARAM1;

	mux_chain = &usb_muxes[port];
	while (mux_chain) {
		if (mux_chain->mux->driver == &kb8010_usb_mux_driver)
			break;
		mux_chain = mux_chain->next;
	}

	if (!mux_chain)
		return EC_ERROR_PARAM1;

	mux = mux_chain->mux;

	/* Validate r/w selection */
	rw = argv[2][0];
	if (rw != 'w' && rw != 'r')
		return EC_ERROR_PARAM2;

	/* Get register address */
	reg = strtoi(argv[3], &e, 0);
	if (*e || reg < 0)
		return EC_ERROR_PARAM3;
	rv = EC_SUCCESS;
	if (rw == 'r')
		rv = kb8010_read(mux, reg, &data);
	else {
		if (argc < 5)
			return EC_ERROR_PARAM_COUNT;
		/* Get value to be written */
		val = strtoi(argv[4], &e, 0);
		if (*e || val < 0)
			return EC_ERROR_PARAM4;
		rv = kb8010_write(mux, reg, val);
		if (rv == EC_SUCCESS) {
			rv = kb8010_read(mux, reg, &data);
			if (rv == EC_SUCCESS && data != val)
				rv = EC_ERROR_UNKNOWN;
		}
	}

	if (rv == EC_SUCCESS)
		ccprintf("register 0x%x [%d] = 0x%x [%d]\n", reg, reg, data,
			 data);

	return rv;
}
DECLARE_CONSOLE_COMMAND(kbxfer, console_command_kb8010_xfer,
			"<port> <r/w> <reg> | <val>",
			"Read or write to KB retimer register");
#endif /* CONFIG_CMD_RETIMER */

const struct usb_mux_driver kb8010_usb_retimer_driver = {
	.init = kb8010_init,
	.set = kb8010_set_state,
	.enter_low_power_mode = kb8010_enter_low_power_mode,
};
