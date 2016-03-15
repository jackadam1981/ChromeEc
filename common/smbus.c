/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * smbus cross-platform code for Chrome EC
 *  ref: http://smbus.org/specs/smbus20.pdf
 */

#include "common.h"
#include "console.h"
#include "util.h"
#include "i2c.h"
#include "smbus.h"
#include "crc8.h"
#include "shared_mem.h"

#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* Write 2 bytes using smbus word access protocol */
int smbus_write_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t d16)
{
	uint8_t buf[4];
	int rv;

	i2c_lock(i2c_port, 1);

	buf[0] = smbus_cmd;
	buf[1] = d16 & 0xff;
	buf[2] = (d16 >> 8) & 0xff;
	buf[3] = crc8(&buf[1], 2);
	rv = i2c_xfer(i2c_port, slave_addr, buf, 4, NULL, 0, I2C_XFER_SINGLE);

	i2c_lock(i2c_port, 0);
	return rv;
}

/* Write up to SMBUS_MAX_BLOCK_SIZE bytes using smbus block access protocol */
int smbus_write_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len)
{
	uint8_t buf[2];
	int rv;

	/* Send command + data length */
	buf[0] = smbus_cmd;
	buf[1] = len;
	rv = i2c_xfer(i2c_port, slave_addr, buf, 2, NULL, 0, I2C_XFER_START);
	if (rv != EC_SUCCESS)
		goto smbus_write_block_done;

	/* Write data */
	rv = i2c_xfer(i2c_port, slave_addr, data, len, NULL, 0, 0);
	if (rv != EC_SUCCESS)
		goto smbus_write_block_done;

	/* Write CRC */
	buf[0] = crc8(data, len);
	rv = i2c_xfer(i2c_port, slave_addr, buf, 1, NULL, 0, I2C_XFER_STOP);

smbus_write_block_done:
	i2c_lock(i2c_port, 1);
	return rv;
}

/* Read 2 bytes using smbus word access protocol */
int smbus_read_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t *p16)
{
	/* 2 bytes data + 1 byte crc */
	uint8_t buf[3];
	int rv;

	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, slave_addr,
		&smbus_cmd, 1, buf, 3, I2C_XFER_SINGLE);

	if (crc8(buf, 2) != buf[2])
		rv = EC_ERROR_CRC;

	if (rv == EC_SUCCESS)
		*p16 = (buf[1] << 8) | buf[0];
	else
		*p16 = 0;
	return rv;
}

/* Read up to SMBUS_MAX_BLOCK_SIZE bytes using smbus block access protocol */
int smbus_read_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t *plen)
{
	int rv, read_len;
	uint8_t *buf;

	/* Longest-case read + 1 byte crc */
	rv = shared_mem_acquire(SMBUS_MAX_BLOCK_SIZE + 1, (char **)&buf);

	if (rv) {
		CPRINTF("smbus read block[%02X] mem error\n",
			smbus_cmd);
		return rv;
	}

	i2c_lock(i2c_port, 1);

	/* First read size from slave */
	rv = i2c_xfer(i2c_port, slave_addr,
		      &smbus_cmd, 1, buf, 1, I2C_XFER_START);
	if (rv != EC_SUCCESS)
			goto smbus_read_block_done;

	/* Now read back all bytes + crc */
	read_len = MIN(buf[0], SMBUS_MAX_BLOCK_SIZE);
	rv = i2c_xfer(i2c_port, slave_addr,
		      NULL, 0, buf, read_len + 1, I2C_XFER_STOP);

	if (crc8(buf, read_len) != buf[read_len])
		rv = EC_ERROR_CRC;

smbus_read_block_done:
	if (rv == EC_SUCCESS) {
		*plen = MIN(*plen, read_len);
		memcpy(data, buf, MIN(*plen, read_len));
	} else
		memset(data, 0x0, *plen);

	i2c_lock(i2c_port, 0);
	shared_mem_release(buf);
	return rv;
}

int smbus_read_string(int i2c_port, uint8_t slave_addr, uint8_t smbus_cmd,
			uint8_t *data, uint8_t len)
{
	int rv;
	len -= 1;
	rv = smbus_read_block(i2c_port, slave_addr, smbus_cmd, data, &len);
	data[len] = '\0';
	return rv;
}
