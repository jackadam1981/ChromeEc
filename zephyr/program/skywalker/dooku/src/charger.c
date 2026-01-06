/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "console.h"
#include "driver/charger/bq257x0_regs.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dooku_charger, LOG_LEVEL_INF);

#define BQ257X0_CHARGE_OPTION_1_EN_AUTO_WAKEUP_SHIFT 0
#define BQ257X0_CHARGE_OPTION_1_EN_AUTO_WAKEUP_BITS 1
#define BQ257X0_CHARGE_OPTION_1_EN_AUTO_WAKEUP__DISABLE 0
#define BQ257X0_CHARGE_OPTION_1_EN_AUTO_WAKEUP__ENABLE 1

__override void board_set_charge_limit(int port, int supplier, int charge_ma,
				       int max_ma, int charge_mv)
{
	if (battery_is_present() == BP_YES) {
		/*
		 * Limit current to 98% when AC+DC, for
		 * CONFIG_CHARGER_INPUT_CURRENT_DERATE_PCT is set to 2.
		 */
		charge_set_input_current_limit(charge_ma, charge_mv);
	} else {
		/* Limit current to 100% when AC only */
		charger_set_input_current_limit(0, charge_ma);
	}
}

static void board_charger_init(void)
{
	/*
	 * Enable AUTO_WAKEUP_EN when battery is still in ship mode.
	 */
	if (battery_get_disconnect_state() != BATTERY_DISCONNECTED)
		return;

	LOG_INF("BQ25720: set AUTO_WAKEUP_EN=1");
	i2c_update16(chg_chips[CHARGER_SOLO].i2c_port,
		     chg_chips[CHARGER_SOLO].i2c_addr_flags,
		     BQ25710_REG_CHARGE_OPTION_1,
		     1 << BQ257X0_CHARGE_OPTION_1_EN_AUTO_WAKEUP_SHIFT,
		     MASK_SET);
}
DECLARE_HOOK(HOOK_INIT, board_charger_init, HOOK_PRIO_PRE_DEFAULT);
