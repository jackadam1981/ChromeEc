/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "pi3usb9201.h"
#include "pwr_defs.h"

#define PI3USB9201_ADDR	0x5f

inline void init_pi3usb9201(void)
{
	/*
	 * Write 0x08 (Client mode detection and Enable USB switch auto ON) to
	 *	control Reg 2
	 * Write 0x08 (Client Mode) to Control Reg 1
	 */
	i2c_write16(1, PI3USB9201_ADDR, CTRL_REG1, 0x0808);
}

inline void write_pi3usb9201(enum pi3usb9201_reg_t reg,
					enum pi3usb9201_dat_t dat)
{
	i2c_write8(1, PI3USB9201_ADDR, reg, dat);
}

inline uint8_t read_pi3usb9201(enum pi3usb9201_reg_t reg)
{
	int tmp;

	i2c_read8(1, PI3USB9201_ADDR, reg, &tmp);

	return tmp;
}

int pi3usb9201_get_max_current(struct pwr_con_t *vbus_pwr)
{
	int tmp, rv;

	rv = i2c_read8(1, PI3USB9201_ADDR, CLIENT_STATUS, &tmp);

	if (rv) {
		vbus_pwr->milli_amps = 0;
		return rv;
	}
	switch (tmp) {
	case CS_2_4A_CHARGER:
		vbus_pwr->milli_amps = 2400;
		break;
	case CS_2A_CHARGER:
		vbus_pwr->milli_amps = 2000;
		break;
	case CS_1A_CHARGER:
		vbus_pwr->milli_amps = 1000;
		break;
	case CS_CDP:
	case CS_DCP:
		vbus_pwr->milli_amps = 1500;
		break;
	/*
	 * Max current is actually dependent on the connection type - either
	 * 500mA for USB2 or 900mA for USB3 - and we cannot distinguish this
	 * here. Assume lower value, caller is in a better position to determine
	 * current in such case as well as available voltage.
	 */
	case CS_SDP:
		vbus_pwr->milli_amps = 500;
		break;
	default:
		vbus_pwr->milli_amps = 0;
	}

	return 0;
}
