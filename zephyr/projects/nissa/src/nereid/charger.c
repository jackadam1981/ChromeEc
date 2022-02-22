/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "charge_state_v2.h"
#include "driver/charger/sm5803.h"

#include "extpower.h"
#include "hooks.h"
#include "usb_pd.h"
#include "sub_board.h"

static inline enum ec_error_list chg_read8(int chgnum, int offset, int *value)
{
	return i2c_read8(chg_chips[chgnum].i2c_port,
			 chg_chips[chgnum].i2c_addr_flags,
			 offset, value);
}

static inline enum ec_error_list chg_write8(int chgnum, int offset, int value)
{
	return i2c_write8(chg_chips[chgnum].i2c_port,
			  chg_chips[chgnum].i2c_addr_flags,
			  offset, value);
}

const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_USB_C0_TCPC,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
	/* Sub-board */
	{
		.i2c_port = I2C_PORT_USB_C1_TCPC,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
};

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = sm5803_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Nereid does not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

/*
 * Set input current limit step extension. When bit7 is set,
 * the input current limit step is multiplied by 1.56.
 */
static void set_input_current_extension(void)
{
	int reg_read;
	
	int val;
	int rv;
	int chgnum = 0;

	chgnum = charge_get_active_chg_chip();
	reg_read = chg_read8(chgnum, SM5803_ISO_CL_REG2, &val);
	val |= SM5803_CHG_ILIM_EXTD;
	rv = chg_write8(chgnum, SM5803_ISO_CL_REG2, val);

}
DECLARE_HOOK(HOOK_INIT, set_input_current_extension, HOOK_PRIO_DEFAULT);
