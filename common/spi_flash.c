/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI flash interface for Chrome EC */

#include "common.h"
#include "spi.h"
#include "spi_flash.h"
#include "util.h"

static const uint32_t supported_flash[] = {
	0xef4017, /* Winbond W25Q64BV */
	0xef4018, /* Winbond W25Q128BV */
};

int spi_flash_probe(void)
{
	uint8_t cmd[] = {0x9f};
	uint8_t resp[3];
	uint32_t chip_id;
	int i, r;

	spi_enable(1);

	r = spi_transaction(cmd, 1, resp, 3);

	spi_enable(0);

	if (r != EC_SUCCESS)
		return r;

	chip_id = (resp[0] << 16) | (resp[1] << 8) | resp[2];

	for (i = 0; i < ARRAY_SIZE(supported_flash); ++i)
		if (chip_id == supported_flash[i])
			return EC_SUCCESS;
	return EC_ERROR_UNKNOWN;
}

int spi_flash_read(uint32_t src_addr, uint8_t *dest, int size)
{
	uint8_t cmd[] = {0x03, src_addr >> 16, src_addr >> 8, src_addr};
	int r;

	spi_enable(1);

	r = spi_transaction(cmd, 4, dest, size);

	spi_enable(0);

	return r;
}
