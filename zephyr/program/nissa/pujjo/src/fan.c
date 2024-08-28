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
static uint32_t sku_id;
static uint32_t fw_config;

int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	LOG_INF("cbi_board_override %d", tag);
	if (override_flag == OVERRIDE_15W) {
		if (tag == CBI_TAG_SKU_ID) {
			switch (sku_id) {
			case 0xa0012:
				sku_id = 0xa0054;
				break;
			case 0xa0013:
				sku_id = 0xa0056;
				break;
			case 0xa0015:
				sku_id = 0xa005a;
				break;
			case 0xa0016:
				sku_id = 0xa005c;
				break;
			case 0xa002a:
				sku_id = 0xa0055;
				break;
			case 0xa002b:
				sku_id = 0xa0057;
				break;
			case 0xa002d:
				sku_id = 0xa005b;
				break;
			case 0xa002e:
				sku_id = 0xa005d;
				break;
			default:
				break;
			}
			LOG_INF("rewrite skuid 0x%x", sku_id);
			memcpy(buf, &sku_id, *size);
		}
		if (tag == CBI_TAG_FW_CONFIG) {
			fw_config |= 0x4;
			LOG_INF("rewrite fwconfig 0x%x", fw_config);
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
		dptf_set_fan_duty_target(0);
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
	hook_call_deferred(&check_fan_status_data, 300 * MSEC);
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
		ret = cbi_get_board_version(&val);
		if (ret != 0) {
			return;
		}
		if (val == 3) {
			ret = cbi_get_sku_id(&sku_id);
			if (ret != 0) {
				return;
			}
			ret = cbi_get_fw_config(&fw_config);
			if (ret != 0) {
				return;
			}
			if (override_flag != 0)
				return;
			switch (sku_id) {
			case 0xa0012:
			case 0xa0013:
			case 0xa0015:
			case 0xa0016:
			case 0xa002a:
			case 0xa002b:
			case 0xa002d:
			case 0xa002e:
				/* Configure the fan enable GPIO */
				gpio_pin_configure_dt(
					GPIO_DT_FROM_NODELABEL(gpio_fan_enable),
					GPIO_OUTPUT);
				hook_call_deferred(&set_fan_status_data,
						   100 * MSEC);
				break;
			default:
				/* Disable the fan */
				fan_set_count(0);
				LOG_INF("Override false1");
				override_flag = OVERRIDE_6W;
				break;
			}
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
