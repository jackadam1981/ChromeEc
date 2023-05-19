/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_app_main.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "host_command.h"
#include "include/power_button.h"
#include "lid_switch.h"
#include "power.h"
#include "power/amd_x86.h"
#include "task.h"

#include <setjmp.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

#include <dt-bindings/buttons.h>

/* All emulated GPIOS are on one device */
#define GPIO_DEVICE \
	DEVICE_DT_GET(DT_GPIO_CTLR(NAMED_GPIOS_GPIO_NODE(s0_pgood), gpios))
#define SLP_S3_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(slp_s3_l), gpios)
#define SLP_S5_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(slp_s5_l), gpios)
#define PGOOD_S0_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(s0_pgood), gpios)
#define PGOOD_S5_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(pg_pwr_s5), gpios)

/*
 * Provide standard array of power signals for the module based on our DTS enum
 * names we filled in
 */
const struct power_signal_info power_signal_list[] = {
	[X86_SLP_S3_N] = {
		.gpio = GPIO_PCH_SLP_S3_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S3_DEASSERTED",
	},
	[X86_SLP_S5_N] = {
		.gpio = GPIO_PCH_SLP_S5_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S5_DEASSERTED",
	},
	[X86_S0_PGOOD] = {
		.gpio = GPIO_S0_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "S0_PGOOD",
	},
	[X86_S5_PGOOD] = {
		.gpio = GPIO_S5_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "S5_PGOOD",
	},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

static int chipset_reset_count;

static void do_chipset_reset(void)
{
	chipset_reset_count++;
}
DECLARE_HOOK(HOOK_CHIPSET_RESET, do_chipset_reset, HOOK_PRIO_DEFAULT);

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, system_can_boot_ap);
FAKE_VALUE_FUNC(int, battery_wait_for_stable);
int battery_is_present(void)
{
	return 1;
}

void amd_power_before(void *fixture)
{
	static const struct device *gpio_dev = GPIO_DEVICE;

	RESET_FAKE(system_can_boot_ap);
	system_can_boot_ap_fake.return_val = 1;

	zassert_ok(gpio_emul_input_set(gpio_dev, SLP_S5_PIN, 0));
	zassert_ok(gpio_emul_input_set(gpio_dev, SLP_S3_PIN, 0));
	zassert_ok(gpio_emul_input_set(gpio_dev, PGOOD_S0_PIN, 0));
	zassert_ok(gpio_emul_input_set(gpio_dev, PGOOD_S5_PIN, 0));
	power_set_state(POWER_G3);
	task_wake(TASK_ID_CHIPSET);
	k_sleep(K_MSEC(500));
	zassert_equal(power_get_state(), POWER_G3, "power_state=%d",
		      power_get_state());
	zassert_equal(power_has_signals(POWER_SIGNAL_MASK(0)), 0);
}

void amd_power_after(void *fixture)
{
	host_clear_events(EC_HOST_EVENT_MASK(EC_HOST_EVENT_HANG_DETECT));
	system_clear_reset_flags(EC_RESET_FLAG_SYSJUMP | EC_RESET_FLAG_AP_OFF);
}

ZTEST_SUITE(amd_power, NULL, NULL, amd_power_before, amd_power_after, NULL);

ZTEST(amd_power, test_power_chipset_init_ap_off)
{
	system_set_reset_flags(EC_RESET_FLAG_AP_OFF);
	zassert_equal(power_chipset_init(), POWER_G3);
	power_set_state(POWER_G3);

	task_wake(TASK_ID_CHIPSET);
	k_sleep(K_MSEC(500));
	zassert_equal(power_get_state(), POWER_G3, "power_state=%d",
		      power_get_state());
}

void test_main(void)
{
	ec_app_main();
	/*
	 * Fake sleep long enough to ensure all automatic power sequencing is
	 * done
	 */
	k_sleep(K_SECONDS(11));

	ztest_run_test_suites(NULL);

	ztest_verify_all_test_suites_ran();
}
