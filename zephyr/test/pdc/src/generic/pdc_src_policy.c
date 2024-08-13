/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usbc/utils.h"

#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(pdc_src_policy);

#define PDC_NODE_PORT0 DT_NODELABEL(pdc_emul1)
#define PDC_NODE_PORT1 DT_NODELABEL(pdc_emul2)

#define TEST_USBC_PORT0 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT0, pdc)
#define TEST_USBC_PORT1 USBC_PORT_FROM_DRIVER_NODE(PDC_NODE_PORT1, pdc)

static void pdc_src_policy_setup(void)
{
}

ZTEST_SUITE(pdc_src_policy, NULL, pdc_src_policy_setup, NULL, NULL, NULL);

ZTEST_USER(pdc_src_policy, test_src_policy_unattached)
{
	const struct device *pdc0 = DEVICE_DT_GET(PDC_NODE_PORT0);
	const struct device *pdc1 = DEVICE_DT_GET(PDC_NODE_PORT1);

	zassert_true(device_is_ready(pdc0));
	zassert_true(device_is_ready(pdc1));

	LOG_INF("PDC ports are ready");
	LOG_INF("1st PDC test port = %d", TEST_USBC_PORT0);
	LOG_INF("2nd PDC test port = %d", TEST_USBC_PORT1);

	zassert_true(true);
}
