/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA9675PW I/O Port expander driver source
 */

/* TODO (b/169814014): Implement code to fit "struct ioexpander_drv" */

#include "pca9675.h"

static int pca9675_read(int ioex, uint16_t *data)
{
	return i2c_xfer(pca9675_iox[ioex].i2c_host_port,
			pca9675_iox[ioex].i2c_addr_flags,
			NULL, 0, (uint8_t *)data, 2);
}

static int pca9675_write(int ioex, uint16_t data)
{
	/*
	 * PCA9675 is Quasi-bidirectional I/O architecture hence
	 * append the direction (1 = input, 0 = output) to prevent
	 * overwriting I/O pins indvertantly.
	 */
	data |= pca9675_iox[ioex].io_direction;

	return i2c_xfer(pca9675_iox[ioex].i2c_host_port,
			pca9675_iox[ioex].i2c_addr_flags,
			(uint8_t *)&data, 2, NULL, 0);
}

int pca9675_get_pin(int ioex, uint16_t pin, bool *level)
{
	int rv;
	uint16_t data_read;

	rv = pca9675_read(ioex, &data_read);
	if (!rv)
		*level = !!(data_read & pin);

	return rv;
}

int pca9675_update_pins(int ioex, uint16_t setpins, uint16_t clearpins)
{
	int rv;
	uint16_t data;

	rv = pca9675_read(ioex, &data);
	if (rv)
		return rv;

	data |= setpins;
	data &= ~clearpins;

	return pca9675_write(ioex, data);
}

int pca9675_init(int ioex)
{
	int rv;

	/* Set pca9675 to Power-on reset */
	rv = pca9675_write(ioex, PCA9675_RESET_SEQ_DATA);
	if (rv)
		return rv;

	/* Initialize the I/O direction */
	return pca9675_write(ioex, 0);
}
