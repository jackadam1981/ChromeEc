/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/gpio/gpio_emul.h>

static const struct device *acok_gpio_dev =
	DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(gpio_acok_od), gpios));
static const gpio_port_pins_t acok_pin =
	DT_GPIO_PIN(DT_NODELABEL(gpio_acok_od), gpios);

void set_ac(int on, bool wait)
{
	gpio_emul_input_set(acok_gpio_dev, acok_pin, on);

	if (wait) {
		k_msleep(CONFIG_PLATFORM_EC_EXTPOWER_DEBOUNCE_MS * 10);
	}
}

/* This is meant to run prior to the ec_app_main() so that the AC OK
 * GPIO level can be set high or low before the extpower_init() routine
 * runs.
 */
static int acok_asserted_init(void)
{
	set_ac(CONFIG_ACOK_INIT_VALUE, false);

	return 0;
}

SYS_INIT(acok_asserted_init, APPLICATION, 50);
