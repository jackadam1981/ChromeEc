/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA9675PW I/O Port expander driver source
 */

/* TODO (b/169814014): Implement code to fit "struct ioexpander_drv" */

#include "pca9675.h"

/* cache the I/O expander GPIO pins */
static uint16_t cache_pin_val;

static int pca9675_read(const int port, const uint16_t i2c_addr_flags,
			       uint16_t *data)
{
	return i2c_xfer(port, i2c_addr_flags, NULL, 0, (uint8_t *)data, 2);
}

static int pca9675_write(const int port, const uint16_t i2c_addr_flags,
				uint16_t data)
{
	return i2c_xfer(port, i2c_addr_flags, (uint8_t *)&data, 2, NULL, 0);
}

int pca9675_get_pin(const int port, const uint16_t i2c_addr_flags,
			uint16_t pin, bool *level)
{
	int rv;
	uint16_t data_read;

	rv = pca9675_read(port, i2c_addr_flags, &data_read);
	if (!rv)
		*level = !!(data_read & pin);

	return rv;
}

int pca9675_update_pins(const int port, const uint16_t i2c_addr_flags,
			uint16_t setpins, uint16_t clearpins)
{
	int rv;
	uint16_t val;

	rv = pca9675_read(port, i2c_addr_flags, &val);

	/* Master cache copy needs to be updated only if there is
	 * no i2c error.
	 */
	if (rv == EC_SUCCESS)
		cache_pin_val = val;

	cache_pin_val |= setpins;
	cache_pin_val &= ~clearpins;

	return pca9675_write(port, i2c_addr_flags, cache_pin_val);
}

int pca9675_init(int port, const uint16_t i2c_addr_flags)
{
	return pca9675_write(port, i2c_addr_flags, PCA9675_RESET_SEQ_DATA);
}
