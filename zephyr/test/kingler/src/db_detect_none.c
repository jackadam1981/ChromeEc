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
#include "variant_db_detection.h"

static void *db_detection_setup(void)
{
	const struct device *wp_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_ALIAS(gpio_wp), gpios));
	const gpio_port_pins_t wp_pin = DT_GPIO_PIN(DT_ALIAS(gpio_wp), gpios);

	/* Make sure that write protect is disabled */
	zassert_ok(gpio_emul_input_set(wp_gpio, wp_pin, 1), NULL);
	/* Set CBI db_config to DB_NONE. */
	zassert_ok(cbi_set_fw_config(DB_NONE << 0), NULL);
	/* Run init hooks to initialize cbi. */
	hook_notify(HOOK_INIT);
	return NULL;
}

ZTEST_SUITE(db_detection, NULL, db_detection_setup, NULL, NULL, NULL);

/* test none db case */
ZTEST(db_detection, test_db_detect_none)
{
	zassert_equal(CORSOLA_DB_NONE, corsola_get_db_type(), NULL);
}

/* Add the empty function to build pass */
void bmi3xx_interrupt(enum gpio_signal signal)
{
}
