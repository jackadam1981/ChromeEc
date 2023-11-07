/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_cbi.h"
#include "gpio/gpio.h"
#include "hooks.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

#define TOUCH_ENABLE_DELAY_MS (500 * MSEC)

test_export_static int touch_state;

void touch_enable_deferred(void)
{
	LOG_INF("%s: %d", __func__, touch_state);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_touch_en), touch_state);
}
DECLARE_DEFERRED(touch_enable_deferred);

void soc_edp_bl_interrupt(enum gpio_signal signal)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_TOUTH_EN, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_TOUTH_EN);
		return;
	}

	if (val == FW_TOUTH_EN_DISABLE)
		return;

	if (signal != GPIO_SIGNAL(DT_NODELABEL(gpio_soc_edp_bl_en)))
		return;

	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_soc_edp_bl_en))) {
		touch_state = 1;
		hook_call_deferred(&touch_enable_deferred_data,
				   TOUCH_ENABLE_DELAY_MS);
	} else {
		touch_state = 0;
		hook_call_deferred(&touch_enable_deferred_data, 0);
	}
}
