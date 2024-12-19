/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board re-init for Telith board.
 * Telith has only clamshell config.
 */
#include "battery.h"
#include "common.h"
#include "cros_cbi.h"
#include "fan.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

enum battery_present battery_hw_present(void)
{
	const struct gpio_dt_spec *batt_pres;

	batt_pres = GPIO_DT_FROM_NODELABEL(gpio_ec_battery_pres_odl);

	/* The GPIO is low when the battery is physically present */
	return gpio_pin_get_dt(batt_pres) ? BP_NO : BP_YES;
}

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
		/* Disable the fan */
		fan_set_count(0);
	} else {
		/* Configure the fan enable GPIO */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_fan),
			GPIO_OUTPUT);
	}
}
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_POST_FIRST);

static bool key_bl = FW_KB_BL_NOT_PRESENT;
static bool key_numpad = FW_KB_NUMPAD_NOT_PRESENT;
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
		key_bl = FW_KB_BL_PRESENT;
	} else {
		key_bl = FW_KB_BL_NOT_PRESENT;
	}

	ret = cros_cbi_get_fw_config(FW_KB_NUMPAD, &val);
	if (val == FW_KB_NUMPAD_PRESENT) {
		key_numpad = FW_KB_NUMPAD_PRESENT;
	} else {
		key_numpad = FW_KB_NUMPAD_NOT_PRESENT;
	}
}
DECLARE_HOOK(HOOK_INIT, kb_init, HOOK_PRIO_POST_FIRST);

int8_t board_vivaldi_keybd_idx(void)
{
	int kb_status;

	kb_status = (key_numpad << 1) | key_bl;

	switch (kb_status) {
	case 0:
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_0));
	case 1:
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_1));
	case 2:
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_2));
	case 3:
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_3));
	default:
		return DT_NODE_CHILD_IDX(DT_NODELABEL(kbd_config_0));
	}
}

/*
 * Enable interrupts
 */
static void board_init(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
