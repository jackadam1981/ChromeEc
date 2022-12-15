/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

static void usbc_config_before(void *fixture)
{
	ARG_UNUSED(fixture);
}

ZTEST_USER(usbc_config, test_usbc_interrupt_init)
{
//	power_signal_set.custom_fake = power_signal_set_mock;
//	power_signal_get.custom_fake = power_signal_get_mock;
	hook_notify(HOOK_PRIO_POST_I2C);
}

ZTEST_SUITE(usbc_config, NULL, NULL, usbc_config_before, NULL, NULL);
