/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_smart_battery.h"
//#include "test/drivers/test_state.h"
//#include "test/drivers/utils.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define GPIO_BATT_PRES_ODL_PATH NAMED_GPIOS_GPIO_NODE(ec_batt_pres_odl)
#define GPIO_BATT_PRES_ODL_PORT DT_GPIO_PIN(GPIO_BATT_PRES_ODL_PATH, gpios)

FAKE_VALUE_FUNC(int, ppc_vbus_sink_enable, int, int);
FAKE_VALUE_FUNC(int, ppc_vbus_source_enable, int, int);
FAKE_VALUE_FUNC(int, board_set_active_charge_port, int);

struct battery_fixture {
	i2c_common_emul_finish_write_func finish_write_func;
	i2c_common_emul_start_read_func start_read_func;
};

static void *battery_setup(void)
{
	static struct battery_fixture fixture;

	return &fixture;
}

static void battery_after(void *data)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));

	/* Set default state (battery is present) */
	gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 0);

}

ZTEST_SUITE(rex_battery, NULL, battery_setup,
	    NULL, battery_after, NULL);

ZTEST_USER(rex_battery, test_battery_hw_present)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));

	zassert_not_null(dev, NULL);
	/* ec_batt_pres_odl = 0 means battery present. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 0));
	zassert_equal(BP_YES, battery_hw_present());
	/* ec_batt_pres_odl = 1 means battery missing. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 1));
	zassert_equal(BP_NO, battery_hw_present());
}
