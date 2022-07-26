/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board re-init for clamshell board */
#include <zephyr/logging/log.h>

#include "cros_cbi.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "tablet_mode.h"

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

/*
 * Rusty shares the firmware with Steelix.
 * Steelix is convertible but Rusty is clamshell
 * so some functions should be disabled for clamshell.
 */
static void board_setup_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FORM_FACTOR, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FORM_FACTOR);
		return;
	}
	if (val == CLAMSHELL) {
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
		/* TODO: disable unused GPIOs to save power */
	}
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);
