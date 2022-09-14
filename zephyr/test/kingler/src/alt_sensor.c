/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "zephyr/kernel.h"
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "gpio_signal.h"
#include "hooks.h"

static void *use_alt_sensor_setup(void)
{
	const struct device *wp_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_ALIAS(gpio_wp), gpios));
	const gpio_port_pins_t wp_pin = DT_GPIO_PIN(DT_ALIAS(gpio_wp), gpios);

	/* Make sure that write protect is disabled */
	zassert_ok(gpio_emul_input_set(wp_gpio, wp_pin, 1), NULL);
	/* Set SSFC to enable alt sensors. */
	zassert_ok(cbi_set_ssfc(0x12), NULL);
	/* Run init hooks to initialize cbi. */
	hook_notify(HOOK_INIT);

	return NULL;
}

ZTEST_SUITE(use_alt_sensor, NULL, use_alt_sensor_setup, NULL, NULL, NULL);

static void *no_alt_sensor_setup(void)
{
	const struct device *wp_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_ALIAS(gpio_wp), gpios));
	const gpio_port_pins_t wp_pin = DT_GPIO_PIN(DT_ALIAS(gpio_wp), gpios);

	/* Make sure that write protect is disabled */
	zassert_ok(gpio_emul_input_set(wp_gpio, wp_pin, 1), NULL);
	/* Set SSFC to disable alt sensors. */
	zassert_ok(cbi_set_ssfc(0x9), NULL);
	/* Run init hooks to initialize cbi. */
	hook_notify(HOOK_INIT);

	return NULL;
}

ZTEST_SUITE(no_alt_sensor, NULL, no_alt_sensor_setup, NULL, NULL, NULL);

extern bool base_use_alt_sensor;
static int interrupt_id;

void bmi3xx_interrupt(enum gpio_signal signal)
{
	interrupt_id = 1;
}

void lsm6dsm_interrupt(enum gpio_signal signal)
{
	interrupt_id = 2;
}

ZTEST(use_alt_sensor, test_use_alt_sensor)
{
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(base_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(base_imu_int_l), gpios);

	zassert_equal(true, base_use_alt_sensor, NULL);

	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(interrupt_id, 2, "interrupt_id=%d", interrupt_id);
}

ZTEST(no_alt_sensor, test_no_alt_sensor)
{
	const struct device *base_imu_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(base_imu_int_l), gpios));
	const gpio_port_pins_t base_imu_pin =
		DT_GPIO_PIN(DT_NODELABEL(base_imu_int_l), gpios);

	zassert_equal(false, base_use_alt_sensor, NULL);

	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(base_imu_gpio, base_imu_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(interrupt_id, 1, "interrupt_id=%d", interrupt_id);
}
