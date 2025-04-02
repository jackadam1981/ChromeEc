/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "system.h"
#include "charger.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
#include "driver/charger/bq257x0_regs.h"
#endif

/* Trigger shutdown by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
// #ifndef CONFIG_PLATFORM_EC_HIBERNATE_PSL
// 	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_slp_z), 1);
// 	/*
// 	 * The system should hibernate, but there may be
// 	 * a small delay, so return.
// 	 */
// #endif
}

static void pujjocento_common_init(void)
{
#ifdef CONFIG_PLATFORM_EC_CHARGER_BQ25720
	/* b/353712228:
	 * Pujjocento's HW sets external current limit (ILIM_HIZ) to 0.9A.
	 * FW need to disable EXTILIM on boot to allow larger current.
	 */
	i2c_update16(chg_chips[CHARGER_SOLO].i2c_port,
		     chg_chips[CHARGER_SOLO].i2c_addr_flags,
		     BQ25710_REG_CHARGE_OPTION_2,
		     1 << BQ257X0_CHARGE_OPTION_2_EN_EXTILIM_SHIFT, 0);
#endif
}
DECLARE_HOOK(HOOK_INIT, pujjocento_common_init, HOOK_PRIO_PRE_DEFAULT);

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
