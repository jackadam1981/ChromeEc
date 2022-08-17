/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

#include "flash.h"

ZTEST_SUITE(flash_driver, NULL, NULL, NULL, NULL, NULL);

ZTEST(flash_driver, test_flash_known_data)
{
#if 0
	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 8 };

	int crc = cros_crc8(buffer, 10);

	/* Verifies polynomial values of 0x07 representing x^8 + x^2 + x + 1 */
	zassert_equal(crc, 170, "CRC8 hash did not match");
#endif

	int offset = 4;
	int size = 8;
	char data[10];

	int result = crec_flash_physical_write(offset, size, data);

	zassert_equal(result, EC_ERROR_INVAL, "Oh no flash!");
#if 0

	int crec_flash_physical_write(int offset, int size, const char *data)

#endif
}
