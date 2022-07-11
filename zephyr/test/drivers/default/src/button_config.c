/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Unit Tests for watchdog.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "button.h"
#include "button_config.h"
#include "common.h"
#include "ec_tasks.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"

/**
 * @brief TestPurpose: Verify button_config initialization.
 *
 */
ZTEST(button_config, test_button_config)
{
	const struct button_config_v2 *button;

	for (int i = 0; i < BUTTON_CFG_COUNT; i++) {
		button = get_button_cfg(i);

		printf("button[%d]= {%s, %d, %d, {%d, 0x%X}, %d, %d}\n", i,
		       button->name, button->type, button->gpio,
		       button->spec.pin, button->spec.dt_flags,
		       button->debounce_us, button->flags);
	}

	button = get_button_cfg(BUTTON_CFG_POWER_BUTTON);

	zassert_equal(button->type, 0, NULL);
	zassert_equal(button->gpio, GPIO_POWER_BUTTON_L, NULL);
	zassert_equal(button->debounce_us, 30000, NULL);
	zassert_equal(button->flags, 0, NULL);
}

/**
 * @brief Test Suite: Verifies button_config functionality.
 */
ZTEST_SUITE(button_config, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
