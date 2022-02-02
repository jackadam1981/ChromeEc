/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>
#include <drivers/emul.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "test_state.h"

ZTEST(keyboard_scan, test_lid_open)
{
}

ZTEST_SUITE(keyboard_scan, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
