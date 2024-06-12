/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "common.h"
#include "dps.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "util.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/battery.h>

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);
#define SB_AP23A7L 0x00

bool squirtle_is_more_efficient(int curr_mv, int prev_mv, int batt_mv,
				int batt_mw, int input_mw)
{
	int batt_state;

	battery_status(&batt_state);

	/* Choose 15V PDO or higher when battery is full. */
	if ((batt_state & SB_STATUS_FULLY_CHARGED) && (curr_mv >= 15000) &&
	    (prev_mv < 15000 || curr_mv <= prev_mv)) {
		return true;
	} else {
		return ABS(curr_mv - batt_mv) < ABS(prev_mv - batt_mv);
	}
}

__override struct dps_config_t dps_config = {
	.k_less_pwr = 93,
	.k_more_pwr = 96,
	.k_sample = 1,
	.k_window = 3,
	.t_stable = 10 * SECOND,
	.t_check = 5 * SECOND,
	.is_more_efficient = &squirtle_is_more_efficient,
};

enum battery_present battery_is_present(void)
{
	int rv;
	struct battery_static_info *bs = &battery_static[BATT_IDX_MAIN];

	rv = gpio_get_level(GPIO_BATT_PRES_ODL) ? BP_NO : BP_YES;

	if (!strcasecmp(bs->model_ext, "AP23A7L")) {
		/*
		 * According to the battery manufacturer's reply:
		 * To detect a bad battery, need to write 0x54 to the
		 * 0x00 register and then read the 0x23. If the 12th
		 * bit(Permanently Failure) is 1, it means a bad battery.
		 */
		uint8_t state[4];
		uint16_t data;
		uint8_t page = 0x54;

		i2c_write_block(I2C_PORT_BATTERY, BATTERY_ADDR_FLAGS,
				SB_AP23A7L, &page, 1);
		rv &= !(i2c_read_string(I2C_PORT_BATTERY, BATTERY_ADDR_FLAGS,
					SB_MANUFACTURER_DATA, state, 4));
		data = (state[3] << 24) | (state[2] << 16) | (state[1] << 8) |
		       state[0];
		rv &= !((data & BIT(12)) >> 12);
	} else if (!strcasecmp(bs->model_ext, "AP23A8L")) {
		/*
		 * According to the battery manufacturer's reply:
		 * To detect a bad battery, need to read the 0x43 register.
		 * If the second bit(Permanently Failure) is 1, it means a bad
		 * battery.
		 */
		int state;
		rv &= !(sb_read(SB_PACK_STATUS, &state));
		rv &= !((state & BIT(2)) >> 2);
	} else if (!strcasecmp(bs->model_ext, "")) {
		rv = BP_NO;
	}
	return rv;
}
