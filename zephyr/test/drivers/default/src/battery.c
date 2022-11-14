/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_smart_battery.h"
#include "test/drivers/test_state.h"

#define GPIO_BATT_PRES_ODL_PATH DT_PATH(named_gpios, ec_batt_pres_odl)
#define GPIO_BATT_PRES_ODL_PORT DT_GPIO_PIN(GPIO_BATT_PRES_ODL_PATH, gpios)

FAKE_VALUE_FUNC(int, battery2_write_func, const struct emul *, int, uint8_t,
		int, void *);

bool authenticate_battery_type(int index, const char *manuf_name);
extern int battery_fuel_gauge_type_override;

struct battery_fixture {
	struct i2c_common_emul_data *battery_i2c_common;
	i2c_common_emul_finish_write_func finish_write_func;
};

static void *battery_setup(void)
{
	static struct battery_fixture fixture;
	static const struct emul *emul = EMUL_DT_GET(DT_NODELABEL(battery));

	fixture.battery_i2c_common =
		emul_smart_battery_get_i2c_common_data(emul);

	return &fixture;
}

static void battery_before(void *data)
{
	struct battery_fixture *fixture = data;

	RESET_FAKE(battery2_write_func);
	fixture->finish_write_func = fixture->battery_i2c_common->finish_write;
}

static void battery_after(void *data)
{
	struct battery_fixture *fixture = data;
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));

	/* Set default state (battery is present) */
	gpio_emul_input_set(dev, GPIO_BATT_PRES_ODL_PORT, 0);
	battery_fuel_gauge_type_override = -1;

	i2c_common_emul_set_write_func(fixture->battery_i2c_common, NULL, NULL);
	fixture->battery_i2c_common->finish_write = fixture->finish_write_func;
}

ZTEST_SUITE(battery, drivers_predicate_post_main, battery_setup, battery_before,
	    battery_after, NULL);

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

ZTEST_F(battery, test_cutoff)
{
	battery_fuel_gauge_type_override = 1;
	fixture->battery_i2c_common->finish_write = NULL;
	i2c_common_emul_set_write_func(fixture->battery_i2c_common,
				       battery2_write_func, NULL);

	battery2_write_func_fake.return_val = -1;
	zassert_equal(EC_RES_ERROR, board_cut_off_battery());

	battery2_write_func_fake.return_val = 0;
	zassert_ok(board_cut_off_battery());
}
