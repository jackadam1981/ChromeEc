/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "fpsensor_detect.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "zephyr/kernel.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

static void *transport_setup(void)
{
	return NULL;
}

ZTEST_SUITE(transport, NULL, transport_setup, NULL, NULL, NULL);

ZTEST(transport, test_transport_type)
{
	const struct device *transport_sel_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(transport_sel), gpios));
	const struct device *div_highside_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(div_highside), gpios));
	const gpio_port_pins_t transport_sel_pin =
		DT_GPIO_PIN(DT_NODELABEL(transport_sel), gpios);
	const gpio_port_pins_t div_highside_pin =
		DT_GPIO_PIN(DT_NODELABEL(div_highside), gpios);

	/* Set the transport sel pin and make sure div_highside is at the right
	 * state
	 */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 0);
	zassert_equal(get_fp_transport_type(), FP_TRANSPORT_TYPE_UART,
		      "Incorrect transport type");
	zassert_equal(gpio_emul_output_get(div_highside_gpio, div_highside_pin),
		      0);

	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 1);
	zassert_equal(get_fp_transport_type(), FP_TRANSPORT_TYPE_SPI,
		      "Incorrect transport type");
	zassert_equal(gpio_emul_output_get(div_highside_gpio, div_highside_pin),
		      0);
}
