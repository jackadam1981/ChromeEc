/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "keyboard_scan.h"

#include <zephyr/device.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(host_set_single_event, enum host_event_code);
FAKE_VOID_FUNC(system_jumped_late);
FAKE_VALUE_FUNC(uint32_t, system_get_reset_flags);

ZTEST(boot_keys, test_something)
{
	zassert_equal(1, 1);
}

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
}

ZTEST_SUITE(boot_keys, NULL, NULL, reset, reset, NULL);
