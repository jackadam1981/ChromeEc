/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

ZTEST(cbi_flash, test_cbi_flash_is_write_protected)
{
	system_is_locked_fake.return_val = 1;
	zassert_equal(cbi_config.drv->is_protected(), 1);
	zassert_equal(system_is_locked_fake.call_count, 1);
}

ZTEST(cbi_flash, test_cbi_flash_is_write_protected_false)
{
	system_is_locked_fake.return_val = 0;
	zassert_equal(cbi_config.drv->is_protected(), 0);
	zassert_equal(system_is_locked_fake.call_count, 1);
}

ZTEST(cbi_flash, test_cbi_flash_load)
{
	uint8_t data[CBI_IMAGE_SIZE];

	zassert_equal(cbi_config.drv->load(0, data, 0), 0);
}

ZTEST(cbi_flash, test_cbi_flash_store)
{
	uint8_t data[CBI_IMAGE_SIZE];

	zassert_equal(cbi_config.drv->store(data), 0);
}

static void cbi_flash_before(void *fixture)
{
	ARG_UNUSED(fixture);
}

ZTEST_SUITE(cbi_flash, drivers_predicate_post_main, NULL, cbi_flash_before,
	    NULL, NULL);
