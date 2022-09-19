/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "gpio.h"
#include "hooks.h"

FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);

static int fake_cbi_get_board_version(uint32_t *ver)
{
	*ver = CONFIG_TENTACRUEL_BOARD_VERSION;

	return 0;
}

ZTEST(ppc_tentacruel, test_ppc_init)
{
	const struct device *ppc_int_gpio = DEVICE_DT_GET(
			DT_GPIO_CTLR(DT_NODELABEL(usb_c0_ppc_int_odl), gpios));
	const gpio_port_pins_t ppc_int_pin =
			DT_GPIO_PIN(DT_NODELABEL(usb_c0_ppc_int_odl), gpios);

	RESET_FAKE(cbi_get_board_version);
	cbi_get_board_version_fake.custom_fake = fake_cbi_get_board_version;

	hook_notify(HOOK_INIT);

	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 0), NULL);
	k_sleep(K_MSEC(100));
}

ZTEST_SUITE(ppc_tentacruel, NULL, NULL, NULL, NULL, NULL);
