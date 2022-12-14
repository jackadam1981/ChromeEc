/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(not_ready, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(not_ready, bad_tcpc)
{
	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		gpio_port_value_t port_value;

		zassert_ok(gpio_port_get(tcpc_config[i].irq_gpio.port, &port_value), "error accessing tcpc port %i", i);
#if 0
		zassert_true(port_value & GPIO_INT_ENABLE, "error port %i flag should be ~0x%X but is instead 0x%X", i, GPIO_INT_ENABLE, port_value);

		zassert_true(port_value == 0, "error port %i flag should be 0x00 but is instead 0x%X", i, port_value);
#endif		
	}
}
