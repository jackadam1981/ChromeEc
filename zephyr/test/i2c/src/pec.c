/*
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "i2c.h"
#include "i2c/i2c.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c_pec_test_emul.h>
#include <zephyr/ztest.h>

#define PEC_PORT I2C_PORT_EEPROM
#define PEC_ADDR (0x0b | I2C_FLAG_PEC)
#define PEC_EMUL EMUL_DT_GET(DT_NODELABEL(i2c_pec_test))

int platform_ec_i2c_read(const int port, const uint16_t addr_flags, uint8_t reg,
			 uint8_t *in, int in_size);

int platform_ec_i2c_write(const int port, const uint16_t addr_flags,
			  const uint8_t *out, int out_size);

static const uint8_t pec_write_pattern[] = { 0, 16, 0, 1,  2,  3,  4,  5,  6,
					     7, 8,  9, 10, 11, 12, 13, 14, 15 };
static const uint8_t pec_block_pattern[] = { 16, 0, 1,	2,  3,	4,  5,	6, 7,
					     8,	 9, 10, 11, 12, 13, 14, 15 };
static uint8_t buf[sizeof(pec_block_pattern)];

/* Test read and writes with PEC enabled. */
ZTEST_USER(i2c, test_i2c_pec_rw)
{
	int rv;
	int len = 0;

	/* Test basic read and write. */
	rv = platform_ec_i2c_write(PEC_PORT, PEC_ADDR, pec_write_pattern,
				   sizeof(pec_write_pattern));
	zassert_ok(rv);

	memset(buf, 0, sizeof(buf));
	rv = platform_ec_i2c_read(PEC_PORT, PEC_ADDR, 0, buf, sizeof(buf));
	zassert_ok(rv);
	zassert_ok(memcmp(pec_block_pattern, buf, sizeof(pec_block_pattern)));

	i2c_pec_test_euml_reset(PEC_EMUL);

	/* Test block accesses. */
	rv = i2c_write_block(PEC_PORT, PEC_ADDR, 0, pec_block_pattern,
			     sizeof(pec_block_pattern));
	zassert_ok(rv);

	memset(buf, 0, sizeof(buf));
	rv = i2c_read_sized_block(PEC_PORT, PEC_ADDR, 0, buf, sizeof(buf),
				  &len);
	zassert_ok(rv);
	zassert_equal(len, 16);
	/* +/- 1 because i2c_read_sized_block consumes the length. */
	zassert_ok(memcmp(pec_block_pattern + 1, buf,
			  sizeof(pec_block_pattern) - 1));
}

/* Test read and writes with PEC corruption enabled. */
ZTEST_USER(i2c, test_i2c_pec_corrupt)
{
	int rv;
	int len = 0;

	i2c_pec_test_emul_set_corrupt(PEC_EMUL, true);

	/* Test basic read and write. */
	/*
	 * CRC failure on write can't be distinguised from other errors by the
	 * host.
	 */
	rv = platform_ec_i2c_write(PEC_PORT, PEC_ADDR, pec_write_pattern,
				   sizeof(pec_write_pattern));
	zassert_equal(rv, EC_ERROR_INVAL);

	memset(buf, 0, sizeof(buf));
	rv = platform_ec_i2c_read(PEC_PORT, PEC_ADDR, 0, buf, sizeof(buf));
	zassert_equal(rv, EC_ERROR_CRC);

	i2c_pec_test_euml_reset(PEC_EMUL);
	i2c_pec_test_emul_set_corrupt(PEC_EMUL, true);

	/* Test block accesses. */
	rv = i2c_write_block(PEC_PORT, PEC_ADDR, 0, pec_block_pattern,
			     sizeof(pec_block_pattern));
	zassert_equal(rv, EC_ERROR_INVAL);

	rv = i2c_read_sized_block(PEC_PORT, PEC_ADDR, 0, buf, sizeof(buf),
				  &len);
	zassert_equal(rv, EC_ERROR_CRC);
}
