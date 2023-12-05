/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "keyboard_protocol.h"
#include "led_onoff_states.h"
#include "led_pwm.h"
#include "pwm_mock.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

void form_factor_init(void);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
FAKE_VOID_FUNC(board_set_active_charge_port, int);
FAKE_VOID_FUNC(set_pwm_led_color, enum pwm_led_id, int);

static void uldren_test_before(void *fixture)
{
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(battery_is_present);
	RESET_FAKE(board_set_active_charge_port);
	RESET_FAKE(set_pwm_led_color);
}

ZTEST_SUITE(uldren, NULL, NULL, uldren_test_before, NULL, NULL);

ZTEST(uldren, test_keyboard_configuration)
{
	extern const struct ec_response_keybd_config uldren_kb;

	zassert_equal_ptr(board_vivaldi_keybd_config(), &uldren_kb);
}

ZTEST(uldren, test_led_pwm)
{
	led_set_color_battery(EC_LED_COLOR_WHITE);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, EC_LED_COLOR_WHITE);

	led_set_color_battery(EC_LED_COLOR_AMBER);
	zassert_equal(set_pwm_led_color_fake.arg0_val, PWM_LED0);
	zassert_equal(set_pwm_led_color_fake.arg1_val, EC_LED_COLOR_AMBER);
}
