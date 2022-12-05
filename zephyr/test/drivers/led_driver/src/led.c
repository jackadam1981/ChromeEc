/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_smart_battery.h"
#include "gpio.h"
#include "i2c.h"
#include "include/power.h"
#include "led.h"
#include "led_common.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <dt-bindings/battery.h>

#define BATTERY_NODE DT_NODELABEL(battery)

#define VERIFY_LED_COLOR(color, led_id)                                    \
	{                                                                  \
		const struct led_pins_node_t *pin_node =                   \
			led_get_node(color, led_id);                       \
		for (int j = 0; j < pin_node->pins_count; j++) {           \
			int val = gpio_pin_get_dt(gpio_get_dt_spec(        \
				pin_node->gpio_pins[j].signal));           \
			int expecting = pin_node->gpio_pins[j].val;        \
			zassert_equal(expecting, val, "[%d]: %d != %d", j, \
				      expecting, val);                     \
		}                                                          \
	}

ZTEST_SUITE(led_driver, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(led_driver, test_led_control)
{
	test_set_chipset_to_power_level(POWER_S5);

	/* Exercise valid led_id, set to RESET state */
	led_control(EC_LED_ID_SYSRQ_DEBUG_LED, LED_STATE_RESET);
	VERIFY_LED_COLOR(LED_OFF, EC_LED_ID_SYSRQ_DEBUG_LED);

	/* Exercise valid led_id, set to OFF state.
	 * Verify matches OFF color defined in device tree
	 */
	led_control(EC_LED_ID_SYSRQ_DEBUG_LED, LED_STATE_OFF);
	VERIFY_LED_COLOR(LED_OFF, EC_LED_ID_SYSRQ_DEBUG_LED);

	/* Exercise valid led_id, set to ON state.
	 * Verify matches ON color defined in device tree
	 */
	led_control(EC_LED_ID_SYSRQ_DEBUG_LED, LED_STATE_ON);
	VERIFY_LED_COLOR(LED_BLUE, EC_LED_ID_SYSRQ_DEBUG_LED);

	/* Exercise invalid led_id -- no change to led color */
	led_control(EC_LED_ID_LEFT_LED, LED_STATE_RESET);
	VERIFY_LED_COLOR(LED_BLUE, EC_LED_ID_SYSRQ_DEBUG_LED);
}

ZTEST(led_driver, test_led_brightness)
{
	uint8_t brightness[EC_LED_COLOR_COUNT] = { -1 };

	/* Verify LED set to OFF */
	led_set_brightness(EC_LED_ID_SYSRQ_DEBUG_LED, brightness);
	VERIFY_LED_COLOR(LED_OFF, EC_LED_ID_SYSRQ_DEBUG_LED);

	/* Verify LED colors defined in device tree are reflected in the
	 * brightness array.
	 */
	led_get_brightness_range(EC_LED_ID_SYSRQ_DEBUG_LED, brightness);
	zassert_equal(brightness[EC_LED_COLOR_BLUE], 1);
	zassert_equal(brightness[EC_LED_COLOR_WHITE], 1);

	/* Verify LED set to WHITE */
	led_set_brightness(EC_LED_ID_SYSRQ_DEBUG_LED, brightness);
	VERIFY_LED_COLOR(LED_WHITE, EC_LED_ID_SYSRQ_DEBUG_LED);
}

ZTEST(led_driver, test_get_chipset_state)
{
	enum power_state pwr_state;

	test_set_chipset_to_g3();
	pwr_state = get_chipset_state();
	zassert_equal(pwr_state, POWER_S5, "expected=%d, returned=%d", POWER_S5,
		      pwr_state);

	test_set_chipset_to_s0();
	pwr_state = get_chipset_state();
	zassert_equal(pwr_state, POWER_S0, "expected=%d, returned=%d", POWER_S0,
		      pwr_state);

	test_set_chipset_to_power_level(POWER_S3);
	pwr_state = get_chipset_state();
	zassert_equal(pwr_state, POWER_S3, "expected=%d, returned=%d", POWER_S3,
		      pwr_state);
}

ZTEST(led_driver, test_get_battery_state)
{
	struct sbat_emul_bat_data *bat;
	const struct emul *emul = EMUL_DT_GET(BATTERY_NODE);
	int status;

	set_ac_enabled(true);

	bat = sbat_emul_get_bat_data(emul);

	zassert_equal(EC_SUCCESS, battery_status(&status));
	zassert_ok(!status, "Battery status: %d", status);
}
