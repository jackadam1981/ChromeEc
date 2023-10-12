/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "drivers/one_wire_uart.h"
#include "drivers/one_wire_uart_internal.h"
#include "timer.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

const static struct device *dev = DEVICE_DT_GET(DT_NODELABEL(one_wire_uart));

FAKE_VALUE_FUNC(int, mkbp_keyboard_add, const uint8_t *);

static void keyboard_before(void *fixture)
{
	one_wire_uart_reset(dev);

	RESET_FAKE(mkbp_keyboard_add);
	one_wire_uart_set_callback(dev, NULL);
}

ZTEST_SUITE(one_wire_uart_keyboard, NULL, NULL, keyboard_before, NULL, NULL);
