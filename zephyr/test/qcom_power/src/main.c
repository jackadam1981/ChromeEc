/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "gpio_signal.h"
#include "power/qcom.h"
#include "battery.h"
#include "ec_app_main.h"

#define AP_RST_L_NODE DT_PATH(named_gpios, ap_rst_l)


ZTEST(qcom_power, test_notify_chipset_reset)
{
	static const struct device *gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(AP_RST_L_NODE, gpios));

	// preconditions
	// chipset_in_state(CHIPSET_STATE_SUSPEND)
	power_signal_enable_interrupt(GPIO_AP_RST_L);

	// Pulse gpio_ap_rst_l 3 times
	zassume_ok(gpio_emul_input_set(gpio_dev, DT_GPIO_PIN(AP_RST_L_NODE, gpios), 1));
	zassume_ok(gpio_emul_input_set(gpio_dev, DT_GPIO_PIN(AP_RST_L_NODE, gpios), 0));

	// asserts
	// "AP_RST_L transitions not expected" not logged
	// power_set_host_sleep_state(HOST_SLEEP_EVENT_DEFAULT_RESET): Maybe check if we exited s3?
}

ZTEST_SUITE(qcom_power, NULL, NULL, NULL, NULL, NULL);

/* Wait until battery is totally stable */
int battery_wait_for_stable(void)
{
	return EC_SUCCESS;
}

void test_main(void)
{
	ec_app_main();
	// Fake sleep long enough to go to S5 and back to G3 again.
	k_sleep(K_SECONDS(11));

	ztest_run_test_suites(NULL);

	ztest_verify_all_test_suites_ran();
}
