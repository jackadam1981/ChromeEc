/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "hooks.h"
#include "system.h"
#include "tablet_mode.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(uldrenite, CONFIG_LOG_DEFAULT_LEVEL);

/* Trigger shutdown by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
#ifndef CONFIG_PLATFORM_EC_HIBERNATE_PSL
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_slp_z), 1);
	/*
	 * The system should hibernate, but there may be
	 * a small delay, so return.
	 */
#endif
}

static void gmr_tablet_init(void)
{
	static int sensor_fwconfig;
	int ret;

	ret = cros_cbi_get_fw_config(FORM_FACTOR, &sensor_fwconfig);
	if (ret < 0) {
		LOG_ERR("error retriving CBI config: %d", ret);
		return;
	}

	if (sensor_fwconfig == FORM_FACTOR_CLAMSHELL) {
		gmr_tablet_switch_disable();
	}
}

DECLARE_HOOK(HOOK_INIT, gmr_tablet_init, HOOK_PRIO_DEFAULT);
