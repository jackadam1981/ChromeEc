/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "cros_cbi.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_backlight.h"
#include "keyboard_config.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(board_init, LOG_LEVEL_INF);
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

static bool has_backlight = FW_KB_BL_NOT_PRESENT;
int8_t board_vivaldi_keybd_idx(void)
{
	if (has_backlight == FW_KB_BL_NOT_PRESENT) {
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_0));
	} else {
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_1));
	}
}

/*
 * Keyboard function decided by FW config.
 */
test_export_static void kb_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_BL, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_KB_BL);
		return;
	}

	if (val == FW_KB_BL_PRESENT) {
		LOG_INF("CBI FW_CONFIG: FW_KB_BL_PRESENT.");
		has_backlight = FW_KB_BL_PRESENT;
	} else {
		LOG_INF("CBI FW_CONFIG: FW_KB_BL_NOT_PRESENT.");
		has_backlight = FW_KB_BL_NOT_PRESENT;
		kblight_enable(0);
	}
}
DECLARE_HOOK(HOOK_INIT, kb_init, HOOK_PRIO_POST_I2C);

/*
 * Keyboard function decided by FW config.
 */
test_export_static void kb_layout_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_LAYOUT, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_KB_LAYOUT field %d",
			FW_KB_LAYOUT);
		return;
	}
	/*
	 * If keyboard is US2(FW_KB_LAYOUT_US2), we need translate right ctrl
	 * to backslash(\|) key.
	 */
	if (val == FW_KB_LAYOUT_US2) {
		set_scancode_set2(3, 14, get_scancode_set2(3, 11));
	}
}
DECLARE_HOOK(HOOK_INIT, kb_layout_init, HOOK_PRIO_POST_I2C);

int board_discharge_on_ac(int enable)
{
	LOG_INF("Kaladin: discharge on AC: %d", enable);

	int port;

	if(enable) {
		port = charge_manager_get_active_charge_port();
		if(port == 0) {
			LOG_INF("discharge on AC port: %d", port);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa3), 1);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa6), 0);
		}else if(port == 1) {
			LOG_INF("discharge on AC port: %d", port);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa3), 0);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa6), 1);
		}else {
			LOG_INF("Unknown charge port");
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa3), 0);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa6), 0);
		}
	}else {
		LOG_INF("Disable discharge on AC");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa3), 0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_gpa6), 0);
	}

	return EC_SUCCESS;
}

__override uint32_t board_override_feature_flags0(uint32_t flags0)
{
	/*
	 * Remove keyboard backlight feature for devices that don't support it.
	 */

	if (has_backlight == FW_KB_BL_NOT_PRESENT)
		return (flags0 & ~EC_FEATURE_MASK_0(EC_FEATURE_PWM_KEYB));
	else
		return flags0;
}
