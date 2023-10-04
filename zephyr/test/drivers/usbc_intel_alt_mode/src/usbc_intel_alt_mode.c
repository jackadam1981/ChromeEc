/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "ec_tasks.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <stdint.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include <gpio.h>

__override uint8_t board_get_usb_pd_port_count(void)
{
	return 1;
}

ZTEST(usbc_intel_altmode, test0)
{
}

ZTEST_SUITE(usbc_intel_altmode, NULL, NULL, NULL, NULL, NULL);
