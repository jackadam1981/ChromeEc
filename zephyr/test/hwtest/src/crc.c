/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crc.h"
#include "crc8.h"
#include "common.h"
#include "zephyr/kernel.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(crc, NULL, NULL, NULL, NULL, NULL);

/* Cover only CRC funcions used with Zephy*/

ZTEST(crc, test_cros_crc8)
{
	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 8 };

	int crc = cros_crc8(buffer, 10);

	/* Verifies polynomial values of 0x07 representing x^8 + x^2 + x + 1 */
	zassert_equal(crc, 170);
}

ZTEST(crc, test_cros_crc16)
{
	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 8 };

	int crc = cros_crc16(buffer, 10, 0);

	/*
	 * Verifies polynomial values of 0x1021 representing X^16 + X^15 + X^2 +
	 * 1
	 */
	zassert_equal(crc, 60681);
}

ZTEST(crc, test_cros_crc8_arg)
{
	uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 8 };

	int crc = cros_crc8_arg(buffer, 10, 234);

	/* Verifies polynomial values of 0x07 representing x^8 + x^2 + x + 1 */
	zassert_equal(crc, 218);
}
