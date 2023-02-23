/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "flash.h"
#include "test/drivers/test_state.h"

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, crec_flash_physical_read, int, int, char *);
FAKE_VALUE_FUNC(int, crec_flash_physical_write, int, int, const char *);

int mock_crec_flash_physical_read(int offset, int size, char *data)
{
	return 0;
}

int mock_crec_flash_physical_write(int offset, int size, const char *data)
{
	return 0;
}

ZTEST(cbi_flash, test_cbi_flash_is_write_protected)
{
	zassert_true(cbi_config.drv->is_protected());
}

ZTEST(cbi_flash, test_cbi_flash_load)
{
	crec_flash_physical_read_fake.custom_fake =
		mock_crec_flash_physical_read;

	uint8_t data[6];

	zassert_equal(cbi_config.drv->load(0, data, 0), 0);
	zassert_equal(1, crec_flash_physical_read_fake.call_count);
}

ZTEST(cbi_flash, test_cbi_flash_store)
{
	crec_flash_physical_write_fake.custom_fake =
		mock_crec_flash_physical_write;

	uint8_t data[6];

	zassert_equal(cbi_config.drv->store(data), 0);
	zassert_equal(0, crec_flash_physical_write_fake.call_count);
}

static void cbi_flash_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(crec_flash_physical_read);
	RESET_FAKE(crec_flash_physical_write);
}

ZTEST_SUITE(cbi_flash, drivers_predicate_post_main, NULL, cbi_flash_before,
	    NULL, NULL);
