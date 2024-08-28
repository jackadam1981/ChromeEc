/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "dptf.h"
#include "fan.h"
#include "gpio/gpio.h"
#include "hooks.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, LOG_LEVEL_INF);

enum override_status {
	OVERRIDE_NONE,
	OVERRIDE_15W,
	OVERRIDE_6W,
};
static int override_flag = OVERRIDE_NONE;

int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	uint32_t id = 0xa00ab;
	uint32_t fw_config = 0x20029017;
	LOG_INF("cbi_board_override %d", tag);
	if (override_flag == OVERRIDE_15W) {
		if (tag == CBI_TAG_SKU_ID) {
			LOG_INF("rewrite skuid");
			memcpy(buf, &id, *size);
		}
		if (tag == CBI_TAG_FW_CONFIG) {
			LOG_INF("rewrite fwconfig");
			memcpy(buf, &fw_config, *size);
		}
	}
	return EC_SUCCESS;
}

void check_fan_status(void)
{
	int rpm;

	rpm = fan_get_rpm_actual(0);
	if (rpm != 0) {
		LOG_INF("Override true");
		override_flag = OVERRIDE_15W;
	} else {
		/* Disable the fan */
		fan_set_count(0);
		LOG_INF("Override false2");
		override_flag = OVERRIDE_6W;
	}
	set_thermal_control_enabled(0, 1);
}
DECLARE_DEFERRED(check_fan_status);

void set_fan_status(void)
{
	dptf_set_fan_duty_target(100);
	hook_call_deferred(&check_fan_status_data, 100 * MSEC);
}
DECLARE_DEFERRED(set_fan_status);

/*
 * Pujjo fan support
 */
test_export_static void fan_init(void)
{
	int ret;
	uint32_t val;
	/*
	 * Retrieve the fan config.
	 */
	ret = cros_cbi_get_fw_config(FW_FAN, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_FAN);
		return;
	}
	if (val != FW_FAN_PRESENT) {
		ret = cbi_get_sku_id(&val);
		if (ret != 0) {
			LOG_ERR("Error retrieving CBI FW_CONFIG field %d",
				FW_FAN);
			return;
		}
		if (override_flag != 0)
			return;
		if (val == 0xa002a) {
			/* Configure the fan enable GPIO */
			gpio_pin_configure_dt(
				GPIO_DT_FROM_NODELABEL(gpio_fan_enable),
				GPIO_OUTPUT);
			hook_call_deferred(&set_fan_status_data, 200 * MSEC);
		} else {
			/* Disable the fan */
			fan_set_count(0);
			LOG_INF("Override false1");
			override_flag = OVERRIDE_6W;
		}
	} else {
		/* Configure the fan enable GPIO */
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_fan_enable),
				      GPIO_OUTPUT);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, fan_init, HOOK_PRIO_DEFAULT + 1);
