/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "ap_power/ap_power.h"
#include "charger.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "ioexpander.h"

void baseboard_suspend_change(struct ap_power_ev_callback *cb,
			      struct ap_power_ev_data data);
void baseboard_init(void);

static void power_signals_before(void *fixture)
{
	ARG_UNUSED(fixture);
}

FAKE_VALUE_FUNC(int, bmi3xx_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(print_charger_prochot, int);

ZTEST_SUITE(power_signals, NULL, NULL, power_signals_before, NULL, NULL);

static int gpio_emul_output_get_dt(const struct gpio_dt_spec *dt)
{
	return gpio_emul_output_get(dt->port, dt->pin);
}

static int gpio_emul_int_enabled_dt(const struct gpio_dt_spec *dt)
{
	int rv;
	gpio_flags_t flags;

	rv = gpio_emul_flags_get(dt->port, dt->pin, &flags);
	if (rv)
		return rv;
	return flags & GPIO_INT_ENABLE;
}

ZTEST(power_signals, test_baseboard_suspend_change)
{
	const struct gpio_dt_spec *gpio_ec_disable_disp_bl
		= GPIO_DT_FROM_NODELABEL(gpio_ec_disable_disp_bl);
	const struct gpio_dt_spec *usb_a1_retimer_en
		= GPIO_DT_FROM_NODELABEL(usb_a1_retimer_en);

	struct ap_power_ev_data data;

	data.event = AP_POWER_SUSPEND;
	baseboard_suspend_change(NULL, data);
	zassert_true(gpio_emul_output_get_dt(gpio_ec_disable_disp_bl));
	zassert_false(gpio_emul_output_get_dt(usb_a1_retimer_en));

	data.event = AP_POWER_RESUME;
	baseboard_suspend_change(NULL, data);
	zassert_false(gpio_emul_output_get_dt(gpio_ec_disable_disp_bl));
	zassert_true(gpio_emul_output_get_dt(usb_a1_retimer_en));
}

ZTEST(power_signals, test_baseboard_init)
{
	const struct gpio_dt_spec *int_pg_groupc_s0
		= GPIO_INT_FROM_NODELABEL(int_pg_groupc_s0);
	const struct gpio_dt_spec *int_pg_lpddr_s0
		= GPIO_INT_FROM_NODELABEL(int_pg_lpddr_s0);
	const struct gpio_dt_spec *int_pg_lpddr_s3
		= GPIO_INT_FROM_NODELABEL(int_pg_lpddr_s3);
	const struct gpio_dt_spec *int_soc_thermtrip
		= GPIO_INT_FROM_NODELABEL(int_soc_thermtrip);
	const struct gpio_dt_spec *int_prochot
		= GPIO_INT_FROM_NODELABEL(int_prochot);
	const struct gpio_dt_spec *int_stb_dump
		= GPIO_INT_FROM_NODELABEL(int_stb_dump);

	baseboard_init();
	zassert_true(gpio_emul_int_enabled_dt(int_pg_groupc_s0));
	zassert_true(gpio_emul_int_enabled_dt(int_pg_lpddr_s0));
	zassert_true(gpio_emul_int_enabled_dt(int_pg_lpddr_s3));
	zassert_true(gpio_emul_int_enabled_dt(int_soc_thermtrip));
	zassert_true(gpio_emul_int_enabled_dt(int_prochot));
	zassert_true(gpio_emul_int_enabled_dt(int_stb_dump));
}