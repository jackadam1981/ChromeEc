/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "usbc/ppc.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);

static int fake_board_version;

static int fake_cbi_get_board_version(uint32_t *ver)
{
	*ver = fake_board_version;
	return 0;
}

FAKE_VOID_FUNC(ppc_chip_0_interrupt, int);
FAKE_VOID_FUNC(ppc_chip_alt_interrupt, int);

ZTEST(ppc_tentacruel, test_ppc_init)
{
	const struct device *ppc_int_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(usb_c0_ppc_int_odl), gpios));
	const gpio_port_pins_t ppc_int_pin =
		DT_GPIO_PIN(DT_NODELABEL(usb_c0_ppc_int_odl), gpios);

	cbi_get_board_version_fake.custom_fake = fake_cbi_get_board_version;

	/* Board version 0, expect that main ppc is enabled. */
	fake_board_version = 0;
	hook_notify(HOOK_INIT);
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(ppc_chip_0_interrupt_fake.call_count, 1, "");
	zassert_equal(ppc_chip_alt_interrupt_fake.call_count, 0, "");

	/* Board version 3, expect that alt ppc is enabled.
	 * Since PPC_ENABLE_ALTERNATE() is not reversible, we must test this
	 * after the board version 0 test.
	 */
	fake_board_version = 3;
	hook_notify(HOOK_INIT);
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(ppc_chip_0_interrupt_fake.call_count, 1, "");
	zassert_equal(ppc_chip_alt_interrupt_fake.call_count, 1, "");
}

static void *ppc_tentacruel_init(void)
{
	/* inject mocked interrupt handlers into ppc_drv and ppc_drv_alt */

	static struct ppc_drv fake_ppc_drv_0;
	static struct ppc_drv fake_ppc_drv_alt;

	fake_ppc_drv_0 = *ppc_chips[0].drv;
	fake_ppc_drv_alt = *ppc_chips_alt[0].drv;
	fake_ppc_drv_0.interrupt = ppc_chip_0_interrupt;
	fake_ppc_drv_alt.interrupt = ppc_chip_alt_interrupt;
	ppc_chips[0].drv = &fake_ppc_drv_0;
	ppc_chips_alt[0].drv = &fake_ppc_drv_alt;

	return NULL;
}

static void ppc_tentacruel_before(void *fixture)
{
	RESET_FAKE(cbi_get_board_version);
	RESET_FAKE(ppc_chip_0_interrupt);
	RESET_FAKE(ppc_chip_alt_interrupt);
}
ZTEST_SUITE(ppc_tentacruel, NULL, ppc_tentacruel_init, ppc_tentacruel_before,
	    NULL, NULL);
