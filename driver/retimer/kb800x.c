/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for Kandou KB800x USB-C 40 Gb/s multiprotocol switch.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "kb800x.h"
#include "time.h"

/* Time between load switch enable and the reset being de-asserted */
#define CONFIG_KB800X_POWER_ON_DELAY_MS 20

mux_state_t cached_mux_state[CONFIG_USB_PD_PORT_MAX_COUNT];

int kb800x_write(const struct usb_mux *me, uint16_t address, uint8_t data)
{
	uint8_t kb800x_config[3] = { 0x00, 0x00, 0x00 };

	kb800x_config[0] = (address >> 8) & 0xff;
	kb800x_config[1] = address & 0xff;
	kb800x_config[2] = data;
	return i2c_xfer(me->i2c_port, me->i2c_addr_flags, kb800x_config, 3,
			NULL, 0);
}

int kb800x_read(const struct usb_mux *me, uint16_t address, uint8_t *data,
		int count)
{
	uint8_t kb800x_config[2] = { 0x00, 0x00 };

	kb800x_config[0] = (address >> 8) & 0xff;
	kb800x_config[1] = address & 0xff;
	return i2c_xfer(me->i2c_port, me->i2c_addr_flags, kb800x_config, 2,
			data, count);
}

#ifdef CONFIG_KB800X_XBAR
/* Assign a phy TX to an elastic buffer */
static void kb800x_set_tx(const struct usb_mux *me,
			  enum kb800x_phy_lane phy_lane, enum kb800x_eb eb)
{
	uint8_t field_value = 0;
	uint8_t regval;

	if (phy_lane <= KB800X_B1) {
		switch (eb) {
		case KB800X_EB1:
			field_value = 4;
			break;
		case KB800X_EB4:
			field_value = 1;
			break;
		case KB800X_EB5:
			field_value = 2;
			break;
		case KB800X_EB6:
			field_value = 3;
			break;
		default:
			break;
		}
	} else {
		switch (eb) {
		case KB800X_EB1:
			field_value = 1;
			break;
		case KB800X_EB2:
			field_value = 2;
			break;
		case KB800X_EB3:
			field_value = 3;
			break;
		case KB800X_EB4:
			field_value = 4;
			break;
		default:
			break;
		}
	}
	/* For lane1 of each PHY, shift by 3 bits */
	field_value <<= (3 * (KB800X_LANE_NUMBER_FROM_PHY(phy_lane)));

	kb800x_read(me, KB800X_REG_TXSEL_FROM_PHY(phy_lane), &regval, 1);
	kb800x_write(me, KB800X_REG_TXSEL_FROM_PHY(phy_lane),
		     regval | field_value);
}

/* Assign a phy RX to an elastic buffer */
static void kb800x_set_rx(const struct usb_mux *me,
			  enum kb800x_phy_lane phy_lane, enum kb800x_eb eb)
{
	uint16_t address = 0;
	uint8_t field_value = 0;
	uint8_t regval = 0;

	switch (phy_lane) {
	case KB800X_A0:
	case KB800X_C0:
		field_value = 1;
		break;
	case KB800X_A1:
	case KB800X_C1:
		field_value = 2;
		break;
	case KB800X_B0:
	case KB800X_D0:
		field_value = 5;
		break;
	case KB800X_B1:
	case KB800X_D1:
		field_value = 6;
		break;
	}
	switch (eb) {
	case KB800X_EB1:
		if (phy_lane >= KB800X_C0)
			field_value <<= 4;
		address = KB800X_REG_XBAR_EB1SEL;
		break;
	case KB800X_EB3:
		field_value <<= 4;
	case KB800X_EB2:
		address = KB800X_REG_XBAR_EB23SEL;
		break;
	case KB800X_EB4:
		if (phy_lane <= KB800X_B1)
			field_value <<= 4;
		address = KB800X_REG_XBAR_EB4SEL;
		break;
	case KB800X_EB6:
		field_value <<= 4;
	case KB800X_EB5:
		address = KB800X_REG_XBAR_EB56SEL;
		break;
	}

	kb800x_read(me, address, &regval, 1);
	kb800x_write(me, address, regval | field_value);
}

static bool kb800x_in_dpmf(const struct usb_mux *me)
{
	if ((cached_mux_state[me->usb_port] & USB_PD_MUX_DP_ENABLED) &&
	    (cached_mux_state[me->usb_port] & USB_PD_MUX_USB_ENABLED))
		return true;
	else
		return false;
}

static bool kb800x_is_dp_lane(const struct usb_mux *me,
			      enum kb800x_ss_lane ss_lane)
{
	if (cached_mux_state[me->usb_port] & USB_PD_MUX_DP_ENABLED) {
		/* DP ALT mode */
		if (kb800x_in_dpmf(me)) {
			/* DPMF pin configuration */
			if ((ss_lane == KB800X_TX1) ||
			    (ss_lane == KB800X_RX1)) {
				return true; /* ML0 or ML1 */
			}
		} else {
			/* Pure, 4-lane DP mode */
			return true;
		}
	}
	/* Not a DP mode or ML2/3 while in DPMF */
	return false;
}

/* Assign SS lane to PHY. Assumes A/B is connector-side, and C/D is host-side */
void kb800x_assign_lane(const struct usb_mux *me, enum kb800x_phy_lane phy_lane,
			enum kb800x_ss_lane ss_lane)
{
	enum kb800x_eb eb = 0;

	/*
	 * Easiest way to handle flipping is to just swap lane 1/0. This assumes
	 * lanes are flipped in the AP. If they are not, they shouldn't be
	 * flipped for the AP-side lanes, but should for connector-side
	 */
	if (cached_mux_state[me->usb_port] & USB_PD_MUX_POLARITY_INVERTED) {
		switch (ss_lane) {
		case KB800X_TX0: /* ML2 / SSTX */
			ss_lane = KB800X_TX1;
			break;
		case KB800X_TX1: /* ML1 */
			ss_lane = KB800X_TX0;
			break;
		case KB800X_RX0: /* ML3 / SSRX */
			ss_lane = KB800X_RX1;
			break;
		case KB800X_RX1: /* ML0 */
			ss_lane = KB800X_RX0;
			break;
		}
	}

	if (kb800x_is_dp_lane(me, ss_lane)) {
		if (kb800x_in_dpmf(me)) {
			/* Route USB3 RX/TX to EB1/4, and ML0/1 to EB5/6 */
			switch (ss_lane) {
			case KB800X_TX1: /* ML1 */
				eb = KB800X_EB6;
				break;
			case KB800X_RX1: /* ML0 */
				eb = KB800X_EB5;
				break;
			default:
				break;
			}
		} else {
			/* Route ML0/1/2/3 through EB1/5/4/6 */
			switch (ss_lane) {
			case KB800X_TX0: /* ML2 */
				eb = KB800X_EB4;
				break;
			case KB800X_TX1: /* ML1 */
				eb = KB800X_EB5;
				break;
			case KB800X_RX0: /* ML3 */
				eb = KB800X_EB6;
				break;
			case KB800X_RX1: /* ML0 */
				eb = KB800X_EB1;
				break;
			}
		}

		/* For DP lanes, always DFP so A/B is TX, C/D is RX */
		if (phy_lane <= KB800X_B1)
			kb800x_set_tx(me, phy_lane, eb);
		else
			kb800x_set_rx(me, phy_lane, eb);
		return;
	}

	/* Lane is either USB3 or CIO */
	if (phy_lane <= KB800X_B1) {
		/* connector-side */
		switch (ss_lane) {
		case KB800X_TX0:
			kb800x_set_tx(me, phy_lane, KB800X_EB4);
			break;
		case KB800X_TX1:
			kb800x_set_tx(me, phy_lane, KB800X_EB5);
			break;
		case KB800X_RX0:
			kb800x_set_rx(me, phy_lane, KB800X_EB1);
			break;
		case KB800X_RX1:
			kb800x_set_rx(me, phy_lane, KB800X_EB2);
			break;
		}
	} else {
		/* host-side */
		switch (ss_lane) {
		case KB800X_TX0:
			kb800x_set_rx(me, phy_lane, KB800X_EB4);
			break;
		case KB800X_TX1:
			kb800x_set_rx(me, phy_lane, KB800X_EB5);
			break;
		case KB800X_RX0:
			kb800x_set_tx(me, phy_lane, KB800X_EB1);
			break;
		case KB800X_RX1:
			kb800x_set_tx(me, phy_lane, KB800X_EB2);
			break;
		}
	}
}

/*
 * Default 'example' mapping, this should be overridden to match the
 * board schematics. If this mapping is used, then CONFIG_KB800X_XBAR
 * should be undefined since a custom xbar mapping isn't needed.
 */
__overridable void board_kb800x_xbar_override(const struct usb_mux *me)
{
	kb800x_assign_lane(me, KB800X_A0, KB800X_TX0);
	kb800x_assign_lane(me, KB800X_A1, KB800X_RX0);

	kb800x_assign_lane(me, KB800X_B0, KB800X_RX1);
	kb800x_assign_lane(me, KB800X_B1, KB800X_TX1);

	kb800x_assign_lane(me, KB800X_C0, KB800X_RX0);
	kb800x_assign_lane(me, KB800X_C1, KB800X_TX0);

	kb800x_assign_lane(me, KB800X_D0, KB800X_TX1);
	kb800x_assign_lane(me, KB800X_D1, KB800X_RX1);
}

static void kb800x_xbar_override(const struct usb_mux *me)
{
	board_kb800x_xbar_override(me);
	kb800x_write(me, KB800X_REG_XBAR_OVR, 0x40);
}
#endif /* CONFIG_KB800X_XBAR */

static void kb800x_global_init(const struct usb_mux *me)
{
	kb800x_write(me, 0x5058, 0x12);
	kb800x_write(me, 0x5059, 0x12);
	kb800x_write(me, 0xFF63, 0x3C);
	kb800x_write(me, 0xF021, 0x02);
	kb800x_write(me, 0xF022, 0x02);
	kb800x_write(me, 0xF057, 0x02);
	kb800x_write(me, 0xF058, 0x02);
	kb800x_write(me, 0x8194, 0x37);
	kb800x_write(me, 0xF0C9, 0x0C);
	kb800x_write(me, 0xF0CA, 0x0B);
	kb800x_write(me, 0xF0CB, 0x0A);
	kb800x_write(me, 0xF0CC, 0x09);
	kb800x_write(me, 0xF0CD, 0x08);
	kb800x_write(me, 0xF0CE, 0x07);
	kb800x_write(me, 0xF0DF, 0x57);
	kb800x_write(me, 0xF0E0, 0x66);
	kb800x_write(me, 0xF0E1, 0x66);
	kb800x_write(me, 0x8198, 0x33);
	kb800x_write(me, 0x8191, 0x00);
}

static void kb800x_usb3_common_init(const struct usb_mux *me)
{
	kb800x_write(me, 0xF020, 0x2f);
	kb800x_write(me, 0xF056, 0x2f);
}

static void kb800x_dp_common_init(const struct usb_mux *me,
				  mux_state_t mux_state)
{
	kb800x_write(me, 0xF2CB, 0x30);
	kb800x_write(me, 0x0011, 0x00);
	kb800x_write(me, KB800X_REG_ORIENTATION,
		     KB800X_ORIENTATION_DP_DFP |
			     ((mux_state & USB_PD_MUX_POLARITY_INVERTED) ?
					    KB800X_ORIENTATION_POLARITY :
					    0x0));
}

static void kb800x_usb3_init(const struct usb_mux *me, mux_state_t mux_state)
{
	kb800x_usb3_common_init(me);

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		kb800x_write(me, KB800X_REG_ORIENTATION,
			     KB800X_ORIENTATION_POLARITY);
}

static void kb800x_dpmf_init(const struct usb_mux *me, mux_state_t mux_state)
{
	kb800x_usb3_common_init(me);
	kb800x_dp_common_init(me, mux_state);

	kb800x_write(me, KB800X_REG_PROTOCOL, KB800X_PROTOCOL_DPMF);
}

static void kb800x_dp_init(const struct usb_mux *me, mux_state_t mux_state)
{
	kb800x_dp_common_init(me, mux_state);

	kb800x_write(me, KB800X_REG_PROTOCOL, KB800X_PROTOCOL_DP);
}

static void kb800x_cio_init(const struct usb_mux *me, mux_state_t mux_state)
{
	uint8_t orientation = 0x0;
	enum idh_ptype cable_type = get_usb_pd_cable_type(me->usb_port);
	union tbt_mode_resp_cable cable_resp = {
		.raw_value =
			pd_get_tbt_mode_vdo(me->usb_port, TCPC_TX_SOP_PRIME)
	};

	kb800x_write(me, 0xF26B, 0x01);
	kb800x_write(me, 0xF26E, 0x19);

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		orientation = KB800X_ORIENTATION_CIO_LANE_SWAP |
			      KB800X_ORIENTATION_POLARITY;

	if (!(mux_state & USB_PD_MUX_USB4_ENABLED)) {
		/* Special configuration only for legacy mode */
		if (((cable_type == IDH_PTYPE_ACABLE) ||
		     cable_resp.tbt_active_passive == TBT_CABLE_ACTIVE)) {
			/* Active cable */
			if (cable_resp.lsrx_comm == UNIDIR_LSRX_COMM) {
				orientation |=
					KB800X_ORIENTATION_CIO_LEGACY_UNIDIR;
			} else {
				/* 'Pre-Coding on a TBT3-Compatible Link' ECN */
				kb800x_write(me, 0x8194, 0x31);
				orientation |=
					KB800X_ORIENTATION_CIO_LEGACY_BIDIR;
			}
		} else {
			/* Passive Cable */
			orientation |= KB800X_ORIENTATION_CIO_LEGACY_PASSIVE;
		}
	}
	kb800x_write(me, KB800X_REG_ORIENTATION, orientation);
	kb800x_write(me, KB800X_REG_PROTOCOL, KB800X_PROTOCOL_CIO);

	/*
	 * Apply pullup to SBRX on host side (C/D side).
	 *
	 * Some CPUs do not apply the required SBTX pullup. In those cases, USB4
	 * won't work unless the retimer applies the pullup to trick itself into
	 * seeing the USB4 router in the AP. This should only be done on the
	 * CPU-facing side, since this is a captive link we are in control of.
	 */
	kb800x_write(me, 0x81fd, 0x08);
	kb800x_write(me, 0x81fe, 0x80);
}

static int kb800x_set_state(const struct usb_mux *me, mux_state_t mux_state)
{
	cached_mux_state[me->usb_port] = mux_state;
	kb800x_write(me, KB800X_REG_RESET, KB800X_RESET_MASK);
	/* Release memory map reset */
	kb800x_write(me, KB800X_REG_RESET,
		     KB800X_RESET_MASK & ~KB800X_RESET_MM);

	/* Already in reset, nothing to do */
	if ((mux_state == USB_PD_MUX_NONE) ||
	    (mux_state & USB_PD_MUX_SAFE_MODE))
		return EC_SUCCESS;

	kb800x_global_init(me);

	/* USB3-only mode */
	if ((mux_state & USB_PD_MUX_USB_ENABLED) &&
	    !(mux_state & USB_PD_MUX_DP_ENABLED))
		kb800x_usb3_init(me, mux_state);

	/* DP alt modes */
	if ((mux_state & USB_PD_MUX_DP_ENABLED)) {
		/* check if DPMF (USB+DP) or just DP */
		if (mux_state & USB_PD_MUX_USB_ENABLED)
			kb800x_dpmf_init(me, mux_state);
		else
			kb800x_dp_init(me, mux_state);
	}

	/* CIO mode (USB4/TBT) */
	if (mux_state &
	    (USB_PD_MUX_USB4_ENABLED | USB_PD_MUX_TBT_COMPAT_ENABLED))
		kb800x_cio_init(me, mux_state);

#ifdef CONFIG_KB800X_XBAR
	kb800x_xbar_override(me);
#endif /* CONFIG_KB800X_XBAR */

	return kb800x_write(me, KB800X_REG_RESET, 0x00);
}

static int kb800x_init(const struct usb_mux *me)
{
	gpio_set_level(kb800x_control[me->usb_port].usb_ls_en_gpio, 1);
	gpio_set_level(kb800x_control[me->usb_port].retimer_rst_gpio, 1);

	msleep(CONFIG_KB800X_POWER_ON_DELAY_MS);

	/* Board will hold down reset signal until power is up */
	if (!gpio_get_level(kb800x_control[me->usb_port].retimer_rst_gpio))
		return EC_ERROR_NOT_POWERED;

	return kb800x_set_state(me, USB_PD_MUX_NONE);
}

static int kb800x_enter_low_power_mode(const struct usb_mux *me)
{
	gpio_set_level(kb800x_control[me->usb_port].retimer_rst_gpio, 0);
	gpio_set_level(kb800x_control[me->usb_port].usb_ls_en_gpio, 0);
	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_RETIMER

static int console_command_kb800x_xfer(int argc, char **argv)
{
	char rw, *e;
	int rv, port, reg, val;
	uint8_t data;
	const struct usb_mux *mux;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;

	/* Get port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || port < 0 || port > board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	mux = &usb_muxes[port];
	while (mux) {
		if (mux->driver == &kb800x_usb_mux_driver)
			break;
		mux = mux->next_mux;
	}

	if (!mux)
		return EC_ERROR_PARAM1;

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
		kb800x_read(mux, reg, &data, 1);
	else {
		/* Get value to be written */
		val = strtoi(argv[4], &e, 0);
		if (*e || val < 0)
			return EC_ERROR_PARAM4;
		kb800x_write(mux, reg, val);
		if (rv == EC_SUCCESS) {
			kb800x_read(mux, reg, &data, 1);
			if (rv == EC_SUCCESS && data != val)
				rv = EC_ERROR_UNKNOWN;
		}
	}

	if (rv == EC_SUCCESS)
		ccprintf("register 0x%x [%d] = 0x%x [%d]\n", reg, reg, data,
			 data);

	return rv;
}
DECLARE_CONSOLE_COMMAND(kbxfer, console_command_kb800x_xfer,
			"<port> <r/w> <reg> | <val>",
			"Read or write to KB retimer register");
#endif /* CONFIG_CMD_RETIMER */

const struct usb_mux_driver kb800x_usb_mux_driver = {
	.init = kb800x_init,
	.set = kb800x_set_state,
	.enter_low_power_mode = kb800x_enter_low_power_mode,
};
