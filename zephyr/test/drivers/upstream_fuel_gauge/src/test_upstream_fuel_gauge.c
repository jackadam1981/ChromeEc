/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "common.h"
#include "console.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_smart_battery.h"
#include "i2c.h"
#include "test/drivers/test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/ztest.h>

#include <stdio.h>

#define BATTERY_NODE DT_NODELABEL(upstream_battery)


ZTEST(upstream_fuel_gauge, test_foo)
{
	printf("hello fuel gauge!\n");
}


ZTEST_SUITE(upstream_fuel_gauge, drivers_predicate_post_main, NULL, NULL,
	    NULL, NULL);
