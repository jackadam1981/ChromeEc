/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "zephyr/kernel.h"
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "gpio_signal.h"
#include "hooks.h"
#include "variant_db_detection.h"

static void *db_detection_setup(void)
{
	const struct device *hdmi_prsnt_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(gpio_hdmi_prsnt_odl), gpios));
	const gpio_port_pins_t hdmi_prsnt_pin =
		DT_GPIO_PIN(DT_NODELABEL(gpio_hdmi_prsnt_odl), gpios);
	/* Set the GPIO to high to indicate the DB is HDMI */
	zassert_ok(gpio_emul_input_set(hdmi_prsnt_gpio, hdmi_prsnt_pin, 0),
		   NULL);

	hook_notify(HOOK_INIT);

	return NULL;
}

ZTEST_SUITE(db_detection, NULL, db_detection_setup, NULL, NULL, NULL);

/* test hdmi db case */
ZTEST(db_detection, test_db_detect_hdmi)
{
	zassert_equal(CORSOLA_DB_HDMI, corsola_get_db_type(), NULL);
}

/* Add the empty function to build pass */
void bmi3xx_interrupt(enum gpio_signal signal)
{
}
