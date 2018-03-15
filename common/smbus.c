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

#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* Write 2 bytes using smbus word access protocol */
int smbus_write_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t d16)
{
	uint8_t buf[5];
	int rv;
	struct i2c_xfer_params p = I2C_XFER_PARAMS_WRITE(i2c_port, slave_addr,
							 buf + 1, 4);

	i2c_lock(i2c_port, 1);

	/* Command sequence for CRC calculation */
	buf[0] = slave_addr;
	buf[1] = smbus_cmd;
	buf[2] = d16 & 0xff;
	buf[3] = (d16 >> 8) & 0xff;
	buf[4] = crc8(buf, 4);
	rv = i2c_xfer(&p);

	i2c_lock(i2c_port, 0);
	return rv;
}

/* Write up to SMBUS_MAX_BLOCK_SIZE bytes using smbus block access protocol */
int smbus_write_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len)
{
	uint8_t buf[3];
	int rv;
	struct i2c_xfer_params p = {
		.port = i2c_port,
		.slave_addr = slave_addr,
	};

	/* Command sequence for CRC calculation */
	buf[0] = slave_addr;
	buf[1] = smbus_cmd;
	buf[2] = len;

	i2c_lock(i2c_port, 1);

	/* Send command + length */
	p.out = buf + 1;
	p.out_size = 2;
	p.flags = I2C_XFER_START;
	rv = i2c_xfer(&p);
	if (rv != EC_SUCCESS)
		goto smbus_write_block_done;

	/* Send data */
	p.out = data;
	p.out_size = len;
	p.flags = 0;
	rv = i2c_xfer(&p);
	if (rv != EC_SUCCESS)
		goto smbus_write_block_done;

	/* Send CRC */
	buf[0] = crc8(buf, 3);
	buf[0] = crc8_arg(data, len, buf[0]);

	p.out = buf;
	p.out_size = 1;
	p.flags = I2C_XFER_STOP;
	rv = i2c_xfer(&p);

smbus_write_block_done:
	i2c_lock(i2c_port, 0);
	return rv;
}

/* Read 2 bytes using smbus word access protocol */
int smbus_read_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t *p16)
{
	uint8_t buf[3];
	int rv;
	uint8_t crc;
	struct i2c_xfer_params p = I2C_XFER_PARAMS(i2c_port, slave_addr,
						   &smbus_cmd,
						   sizeof(smbus_cmd), buf,
						   sizeof(buf),
						   I2C_XFER_SINGLE);

	/* Command sequence for CRC calculation */
	buf[0] = slave_addr;
	buf[1] = smbus_cmd;
	buf[2] = slave_addr | 0x1;
	crc = crc8(buf, 3);

	i2c_lock(i2c_port, 1);

	/* Read data bytes + CRC byte */
	rv = i2c_xfer(&p);

	/* Verify CRC */
	if (crc8_arg(buf, 2, crc) != buf[2])
		rv = EC_ERROR_CRC;

	if (rv == EC_SUCCESS)
		*p16 = (buf[1] << 8) | buf[0];
	else
		*p16 = 0;

	i2c_lock(i2c_port, 0);
	return rv;
}

/* Read up to SMBUS_MAX_BLOCK_SIZE bytes using smbus block access protocol */
int smbus_read_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t *plen)
{
	int rv, read_len;
	uint8_t buf[4];
	uint8_t crc;
	int do_crc = 1;

	struct i2c_xfer_params p = {
		.port = i2c_port,
		.slave_addr = slave_addr,
	};

	/* Command sequence for CRC calculation */
	buf[0] = slave_addr;
	buf[1] = smbus_cmd;
	buf[2] = slave_addr | 0x1;

	i2c_lock(i2c_port, 1);

	/* First read size from slave */
	p.out = &smbus_cmd;
	p.out_size = sizeof(smbus_cmd);
	p.in = buf + 3;
	p.in_size = 1;
	p.flags = I2C_XFER_START;
	rv = i2c_xfer(&p);
	if (rv != EC_SUCCESS)
			goto smbus_read_block_done;
	crc = crc8(buf, 4);

	/*
	 * If our input buffer isn't large enough to hold the entire input,
	 * don't bother verifying crc since it may require reading numerous
	 * data bytes that will be thrown away.
	 */
	read_len = MIN(buf[3], SMBUS_MAX_BLOCK_SIZE);
	if (*plen < read_len) {
		do_crc = 0;
		read_len = *plen;
	}

	/* Now read back all bytes */
	p.out = NULL;
	p.out_size = 0;
	p.in = data;
	p.in_size = read_len;
	p.flags = 0;

	rv = i2c_xfer(&p);
	if (rv)
		goto smbus_read_block_done;

	/* Read CRC + verify */
	p.out = NULL;
	p.out_size = 0;
	p.in = buf;
	p.in_size = 1;
	p.flags = I2C_XFER_STOP;

	rv = i2c_xfer(&p);
	if (do_crc && crc8_arg(data, read_len, crc) != buf[0])
		rv = EC_ERROR_CRC;

smbus_read_block_done:
	if (rv != EC_SUCCESS)
		memset(data, 0x0, *plen);
	else
		*plen = read_len;

	i2c_lock(i2c_port, 0);
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
