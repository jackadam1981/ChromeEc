/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "usbc/ppc.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VOID_FUNC(ppc_chip_0_interrupt, int);
DECLARE_FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config,
			enum cbi_fw_config_field_id, uint32_t *);

static int cros_cbi_get_fw_config_mock(enum cbi_fw_config_field_id field_id,
				       uint32_t *value)
{
	if (field_id != FW_FORM_FACTOR)
		return -EINVAL;

	*value = FW_FORM_FACTOR_CLAMSHELL;

	return 0;
}

ZTEST(ppc_veluza, test_ppc_init)
{
	const struct device *ppc_int_gpio = DEVICE_DT_GET(
		DT_GPIO_CTLR(DT_NODELABEL(usb_c0_ppc_int_odl), gpios));
	const gpio_port_pins_t ppc_int_pin =
		DT_GPIO_PIN(DT_NODELABEL(usb_c0_ppc_int_odl), gpios);

	cros_cbi_get_fw_config_fake.custom_fake = cros_cbi_get_fw_config_mock;

	hook_notify(HOOK_INIT);
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 1), NULL);
	k_sleep(K_MSEC(100));
	zassert_ok(gpio_emul_input_set(ppc_int_gpio, ppc_int_pin, 0), NULL);
	k_sleep(K_MSEC(100));

	zassert_equal(ppc_chip_0_interrupt_fake.call_count, 1);
}

static void *ppc_veluza_init(void)
{
	static struct ppc_drv fake_ppc_drv_0;

	zassert_equal(ppc_cnt, 1);

	/* inject mocked interrupt handlers into ppc_drv */
	fake_ppc_drv_0 = *ppc_chips[0].drv;
	fake_ppc_drv_0.interrupt = ppc_chip_0_interrupt;
	ppc_chips[0].drv = &fake_ppc_drv_0;

	return NULL;
}

static void ppc_veluza_before(void *fixture)
{
	RESET_FAKE(ppc_chip_0_interrupt);
	RESET_FAKE(cros_cbi_get_fw_config);
}
ZTEST_SUITE(ppc_veluza, NULL, ppc_veluza_init, ppc_veluza_before, NULL, NULL);
