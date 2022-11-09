/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "gpio_signal.h"
#include "test/drivers/test_state.h"

void usb0_evt(enum gpio_signal signal);
void usb1_evt(enum gpio_signal signal);

ZTEST_SUITE(pi3usb9201, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(pi3usb9201, test_usb0_evt)
{
	usb0_evt(0);
}

ZTEST(pi3usb9201, test_usb1_evt)
{
	usb1_evt(0);
}
