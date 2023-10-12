/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "drivers/one_wire_uart.h"
#include "drivers/one_wire_uart_internal.h"
#include "usb_hid_touchpad.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

const static struct device *dev = DEVICE_DT_GET(DT_NODELABEL(one_wire_uart));

FAKE_VOID_FUNC(hid_i2c_touchpad_add, const struct usb_hid_touchpad_report *);

static void tablet_before(void *fixture)
{
	one_wire_uart_reset(dev);

	RESET_FAKE(hid_i2c_touchpad_add);
	one_wire_uart_set_callback(dev, NULL);
}

ZTEST_SUITE(one_wire_uart_tablet, NULL, NULL, tablet_before, NULL, NULL);
