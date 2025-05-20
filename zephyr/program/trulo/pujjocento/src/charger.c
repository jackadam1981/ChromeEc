/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
#include "driver/charger/bq257x0_regs.h"
#endif

#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
void update_bq25720_input_voltage(void)
{
	/* b:397587463 set input voltage to 3.2V to prevent charger entering
	 * VINDPM mode */
	i2c_write16(chg_chips[CHARGER_SOLO].i2c_port,
		    chg_chips[CHARGER_SOLO].i2c_addr_flags,
		    BQ25710_REG_INPUT_VOLTAGE, 0);
}
DECLARE_HOOK(HOOK_AC_CHANGE, update_bq25720_input_voltage, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, update_bq25720_input_voltage, HOOK_PRIO_DEFAULT);
#endif
