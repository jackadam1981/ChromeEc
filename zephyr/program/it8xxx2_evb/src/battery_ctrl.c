/*
 * Copyright (c) 2026 ITE Corporation. All Rights Reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "console.h"
#include "crc8.h"
#include "i2c.h"

#include <zephyr/drivers/i2c.h>

static int platform_ec_i2c_write_dma(const int port, const uint16_t addr_flags,
				 const uint8_t *out, int out_size)
{
	if (!IS_ENABLED(CONFIG_SMBUS_PEC) && I2C_USE_PEC(addr_flags))
		return EC_ERROR_UNIMPLEMENTED;

	if (IS_ENABLED(CONFIG_SMBUS_PEC)) {
		int i, rv;
		uint8_t addr_8bit = I2C_STRIP_FLAGS(addr_flags) << 1;
		uint8_t pec;

		pec = cros_crc8(&addr_8bit, 1);
		pec = cros_crc8_arg(out, out_size, pec);

		i2c_lock(port, 1);
		for (i = 0; i <= CONFIG_I2C_NACK_RETRY_COUNT; i++) {

			uint8_t tx_buf[out_size + 1];
			int tx_len = sizeof(tx_buf);

			memcpy(tx_buf, out, out_size);
			tx_buf[out_size] = pec;

			rv = i2c_xfer_unlocked(port, addr_flags, tx_buf, tx_len,
					       NULL, 0, I2C_XFER_START | I2C_XFER_STOP);

			if (!rv)
				break;

		}
		i2c_lock(port, 0);

		return rv;
	}

	return i2c_xfer(port, addr_flags, out, out_size, NULL, 0);
}

int i2c_write16_dma(const int port, const uint16_t addr_flags, int offset, int data)
{
	uint8_t buf[1 + sizeof(uint16_t)];

	buf[0] = offset & 0xff;
	buf[1] = (data >> 8) & 0xff;
	buf[2] = data & 0xff;

	return platform_ec_i2c_write_dma(port, addr_flags, buf,
					 1 + sizeof(uint16_t));
}


