/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PI3HDX1204 retimer.
 */

#include "i2c.h"
#include "pi3hdx1204.h"

int pi3hdx1204_enable(const int i2c_port,
		      const uint16_t i2c_addr_flags,
		      const int enable)
{
	uint8_t buf[PI3HDX1204_ENABLE_OFFSET + 1] = {0};

	buf[PI3HDX1204_ENABLE_OFFSET] =
		enable ? PI3HDX1204_ENABLE_ALL_CHANNELS : 0;

	return i2c_xfer(i2c_port, i2c_addr_flags,
			buf, PI3HDX1204_ENABLE_OFFSET + 1,
			NULL, 0);
}

int pi3hdx1204_write(const int i2c_port,
		      const uint16_t i2c_addr_flags,
		      uint8_t *buf, const int offset)
{
	int rv;
	uint8_t get_buf[offset + 1];

	rv = i2c_read_block(i2c_port, i2c_addr_flags,
			0, get_buf, PI3HDX1204_ENABLE_OFFSET + 1);
	if (rv)
		return rv;

	/* Restore current enable status. */
	buf[PI3HDX1204_ENABLE_OFFSET] =
		get_buf[PI3HDX1204_ENABLE_OFFSET];

	return i2c_xfer(i2c_port, i2c_addr_flags,
			buf, offset + 1,
			NULL, 0);
}

