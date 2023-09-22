/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "string.h"
#include "system.h"
#include "test_util.h"
#include "write_protect.h"

extern int flash_abort_or_invalidate_hash(int offset, int size);

test_static int test_flash_abort_or_invalidate_hash(void)
{
	/* fail if (offset >= CONFIG_RW_MEM_OFF && offset < (CONFIG_RW_MEM_OFF +
	 * CONFIG_RW_SIZE))
	 */
	TEST_EQ(flash_abort_or_invalidate_hash(CONFIG_RW_MEM_OFF, 0),
		EC_ERROR_INVAL, "%d");
	TEST_EQ(flash_abort_or_invalidate_hash(
			CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE - 1, 0),
		EC_ERROR_INVAL, "%d");
	/* fail if ((offset + size) > CONFIG_RW_MEM_OFF && (offset + size) <=
	 * (CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE))
	 */
	TEST_EQ(flash_abort_or_invalidate_hash(CONFIG_RW_MEM_OFF - 64, 128),
		EC_ERROR_INVAL, "%d");
	TEST_EQ(flash_abort_or_invalidate_hash(
			CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE - 64, 128),
		EC_ERROR_INVAL, "%d");
	/* fail if (offset < CONFIG_RW_MEM_OFF && (offset + size) >
	 * (CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE)))
	 */
	TEST_EQ(flash_abort_or_invalidate_hash(CONFIG_RW_MEM_OFF - 64,
					       CONFIG_RW_SIZE + 64 + 1),
		EC_ERROR_INVAL, "%d");

	/* pass in all other cases */
	TEST_EQ(flash_abort_or_invalidate_hash(CONFIG_RW_MEM_OFF - 1024, 512),
		EC_SUCCESS, "%d");
	TEST_EQ(flash_abort_or_invalidate_hash(
			CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE, 512),
		EC_SUCCESS, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_flash_abort_or_invalidate_hash);
	test_print_result();
}
