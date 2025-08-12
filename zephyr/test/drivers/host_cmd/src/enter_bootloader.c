/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VOID_FUNC(chip_enter_bootloader, uint8_t);

const static uint8_t bootloader_mode = 0x43;

void chip_enter_bootloader_custom(uint8_t mode)
{
	zassert_equal(mode, bootloader_mode);
}

ZTEST(hc_enter_bootloader, test_enter_bootloader_ok)
{
	struct ec_params_enter_bootloader params = {
		.mode = bootloader_mode,
	};

	chip_enter_bootloader_fake.custom_fake = chip_enter_bootloader_custom;
	ec_cmd_enter_bootloader(NULL, &params);
}

static void reset(void *data)
{
	ARG_UNUSED(data);

	RESET_FAKE(chip_enter_bootloader);
}

ZTEST_SUITE(hc_enter_bootloader, drivers_predicate_post_main, NULL, reset,
	    reset, NULL);
