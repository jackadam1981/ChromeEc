/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_cbi.h"
#include "hooks.h"

LOG_MODULE_REGISTER(brox_led, LOG_LEVEL_INF);

enum pwr_led_sup {
	PWR_LED_ABSENT,
	PWR_LED_PRESENT,
};
static enum pwr_led_sup pwr_led_support;

void pwr_led_support_check_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_BL, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_KB_BL);
		return;
	}

	if (val == FW_KB_BL_ABSENT)
		pwr_led_support = PWR_LED_ABSENT;
	else
		pwr_led_support = PWR_LED_PRESENT;
}
DECLARE_HOOK(HOOK_INIT, pwr_led_support_check_init, HOOK_PRIO_DEFAULT);

__override int board_led_alt_policy(void)
{
	switch (pwr_led_support) {
	case PWR_LED_ABSENT:
		return 0;
	case PWR_LED_PRESENT:
	default:
		return 1;
	}
}
