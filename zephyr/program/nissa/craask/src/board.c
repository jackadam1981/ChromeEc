/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "gpio/gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "task.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static void shutdown_and_notify(enum ap_power_shutdown_reason reason)
{
	ap_power_force_shutdown(reason);
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN);
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN_COMPLETE);
}

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	int ret;
	static uint32_t sku_id;

	if (sku_id == 0) {
		ret = cbi_get_sku_id(&sku_id);
		if (ret != EC_SUCCESS)
			LOG_ERR("Error retrieving CBI SKU_ID.");
	}

	/* prevent RSMRST drop not fine for SKU_ID 0x140000 ~ 0x140007 */
	if (sku_id == 0x140000 || sku_id == 0x140001 || sku_id == 0x140002 ||
	    sku_id == 0x140003 || sku_id == 0x140004 || sku_id == 0x140005 ||
	    sku_id == 0x140006 || sku_id == 0x140007) {
		if (!power_signal_get(PWR_RSMRST)) {
			LOG_ERR("s0->s3 PWR_RSMRST is not good");
			shutdown_and_notify(AP_POWER_SHUTDOWN_G3);
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);
