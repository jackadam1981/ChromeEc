/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include "panic.h"
#include "sysjump.h"
#include "test/drivers/test_state.h"

static struct jump_data *get_jump_data_ptr(void)
{
	/* This expression gets a pointer to the jump data struct, which is
	 * located at the end of the mock_jump_data region.
	 */
	return (struct jump_data *)(((uint8_t *)&mock_jump_data) + sizeof(mock_jump_data) -
				    sizeof(struct jump_data));
}

ZTEST(panic_output_get_panic_data_write, test_existing_panic_data)
{
	struct panic_data *pdata_actual = test_get_panic_data_pointer();

	/* Pretend panic data exists by setting the magic header and setting its
	 * size.
	 */
	pdata_actual->magic = PANIC_DATA_MAGIC;
	pdata_actual->struct_size = CONFIG_PANIC_DATA_SIZE;

	/* Verify that pdata_ptr is returned */
	zassert_equal(pdata_actual, get_panic_data_write());
}

ZTEST(panic_output_get_panic_data_write, test_no_panic_data__no_jump_data)
{
	struct panic_data *pdata_actual = test_get_panic_data_pointer();
	struct panic_data pdata_expected = {
		.magic = PANIC_DATA_MAGIC,
		.struct_size = CONFIG_PANIC_DATA_SIZE,
	};

	/* Don't fill in any panic data, but add some fake data so we can ensure
	 * it gets reset to zero.
	 */
	pdata_actual->flags = 0xFF;

	/* Verify that pdata_ptr is returned */
	zassert_equal(pdata_actual, get_panic_data_write());

	/* Verify the pdata struct has correct fields filled out. */
	zassert_mem_equal(&pdata_expected, pdata_actual,
			  sizeof(struct panic_data));
}

ZTEST(panic_output_get_panic_data_write, test_no_panic_data__jump_data_v1)
{
	struct panic_data *pdata_actual = test_get_panic_data_pointer();
	struct jump_data *jdata_actual = get_jump_data_ptr();

	/* Set up some jump data */
	jdata_actual->magic = JUMP_DATA_MAGIC;
        jdata_actual->version = 1;
        jdata_actual->reset_flags = 0xAABBCCDD; 

	/* Verify that pdata_ptr is returned */
	zassert_equal(pdata_actual, get_panic_data_write());

}

static void reset(void *data)
{
	ARG_UNUSED(data);

	struct panic_data *pdata = test_get_panic_data_pointer();

	memset(pdata, 0, sizeof(struct panic_data));
}

ZTEST_SUITE(panic_output_get_panic_data_write, drivers_predicate_post_main,
	    NULL, reset, reset, NULL);
