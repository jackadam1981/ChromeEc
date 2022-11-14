/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "test/drivers/test_state.h"

#define GPIO_BATT_PRES_ODL_PATH DT_PATH(named_gpios, ec_batt_pres_odl)
#define GPIO_BATT_PRES_ODL_PORT DT_GPIO_PIN(GPIO_BATT_PRES_ODL_PATH, gpios)

bool authenticate_battery_type(int index, const char *manuf_name);

static void battery_after(void *data)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));

	/* Set default state (battery is present) */
	gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 0);
}

ZTEST_USER(battery, test_battery_is_present_gpio)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));

	zassert_not_null(dev, NULL);
	/* ec_batt_pres_odl = 0 means battery present. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 0));
	zassert_equal(BP_YES, battery_is_present());
	/* ec_batt_pres_odl = 1 means battery missing. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 1));
	zassert_equal(BP_NO, battery_is_present());
}

ZTEST(battery, test_authenticate_battery_type)
{
	/* Invalid index */
	zassert_false(authenticate_battery_type(BATTERY_TYPE_COUNT, NULL));
	/* Use fuel-gauge 1's manufacturer name for index 0 */
	zassert_false(authenticate_battery_type(
		0, board_battery_info[1].fuel_gauge.manuf_name));
	/* Use the correct manufacturer name, but wrong device name (because the
	 * index is 1 and not 0)
	 */
	zassert_false(authenticate_battery_type(
		1, board_battery_info[1].fuel_gauge.manuf_name));
}

ZTEST(battery, test_board_get_default_battery_type)
{
	zassert_equal(DEFAULT_BATTERY_TYPE, board_get_default_battery_type());
}

ZTEST_SUITE(battery, drivers_predicate_post_main, NULL, NULL, battery_after,
	    NULL);
