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
#include "crc.h"
#include "shared_mem.h"

/* smbus interface write n bytes
 *   case 1:  n-1 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A][size][A] ...[Di][Ai]... [PEC][A][P]
 */
int smbus_if_write(int i2c_port, struct smbus_wr_if *intf,
			uint8_t size_n, uint8_t data_n, uint8_t pec_n)
{
	int rv;
	uint8_t n;
	data_n = MIN(data_n, SMBUS_MAX_BLOCK_SIZE);
	n = size_n + data_n + pec_n;
	if (pec_n)
		intf->data[n-1] = crc8(&(intf->slave_addr),
				n - 1 + sizeof(struct smbus_wr_if));
	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr,
			&intf->smbus_cmd, n + 1, NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);
	if (rv)
		cprintf(CC_I2C, "smbus wr i2c_xfer error:%d cmd:%02X n:%d\n",
			rv, intf->smbus_cmd, n);
	return rv;
}

/* smbus interface read n bytes
 *   tx 8-bit smbus cmd, and read n bytes
 *
 *   case 1:  n-1 byte data, 1 byte PEC
 *      [S][Slave Address][Rd=1][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *      [S][Slave Address][Rd=1][A][cmd][A][size][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   Note: slave_addr_rd = ( i2c_addr << 1 ) | (1 bit read mode = 1)
 */
int smbus_if_read(int i2c_port, struct smbus_rd_if *intf,
		uint8_t size_n, uint8_t data_n, uint8_t pec_n)
{
	int rv;
	uint8_t pec, n;
	data_n = MIN(data_n, SMBUS_MAX_BLOCK_SIZE);
	n = size_n + data_n + pec_n;

	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr, &intf->smbus_cmd, 1,
		intf->data, n, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);

	if (rv)
		return rv;

	if (pec_n == 0)
		return EC_SUCCESS;

	intf->slave_addr_rd = intf->slave_addr | 0x01;
	if (size_n)
		data_n = MIN(SMBUS_MAX_BLOCK_SIZE, intf->data[0]);
	n = size_n + data_n + pec_n;
	pec = crc8(&intf->slave_addr, n - 1 + sizeof(struct smbus_rd_if));
	if (pec != intf->data[n-1]) {
		cprintf(CC_I2C, "smbus read[%02X] PEC %02X != %02X\n",
			intf->smbus_cmd, intf->data[n-1], pec);
		return EC_ERROR_PEC;
	}
	return EC_SUCCESS;
}

/* smbus write 2 bytes */
int smbus_write_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t d16)
{
	int rv;
	struct smbus_wr_word *s;
	rv = shared_mem_acquire(sizeof(struct smbus_wr_word), (char **)&s);
	if (rv) {
		cprintf(CC_I2C, "smbus write wd[%02X] mem error\n", smbus_cmd);
		return rv;
	}
	s->slave_addr = slave_addr,
	s->smbus_cmd = smbus_cmd;
	s->data[0] = d16 & 0xFF;
	s->data[1] = (d16 >> 8) & 0xFF;
	rv = smbus_if_write(i2c_port, (struct smbus_wr_if *)s, 0, 2, 1);
	shared_mem_release(s);
	return rv;
}

/* smbus write upto 32 bytes */
int smbus_write_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len)
{
	int rv;
	struct smbus_wr_block *s;
	rv = shared_mem_acquire(sizeof(struct smbus_wr_block), (char **)&s);
	if (rv) {
		cprintf(CC_I2C, "smbus write block[%02X] mem error\n",
			smbus_cmd);
		return rv;
	}
	s->slave_addr = slave_addr,
	s->smbus_cmd = smbus_cmd;
	s->size = MIN(len, SMBUS_MAX_BLOCK_SIZE);
	memmove(s->data, data, s->size);
	rv = smbus_if_write(i2c_port, (struct smbus_wr_if *)s, 1, s->size, 1);
	shared_mem_release(s);
	return rv;
}

/* smbus read 2 bytes */
int smbus_read_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t *p16)
{
	int rv;
	static struct smbus_rd_word s;
	s.slave_addr = slave_addr;
	s.smbus_cmd = smbus_cmd;
	rv = smbus_if_read(i2c_port, (struct smbus_rd_if *)&s, 0, 2, 1);
	if (rv == EC_SUCCESS)
		*p16 = (s.data[1] << 8) | s.data[0];
	return rv;
}
#if 0
int smbus_read_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t *p16)
{
	int rv;
	struct smbus_rd_word *s;
	rv = shared_mem_acquire(sizeof(struct smbus_rd_word), (char **)&s);
	if (rv) {
		cprintf(CC_I2C, "smbus read wd[%02X] mem error\n",
			smbus_cmd);
		return rv;
	}

	s->slave_addr = slave_addr;
	s->smbus_cmd = smbus_cmd;
	rv = smbus_if_read(i2c_port, (struct smbus_rd_if *)s, 0, 2, 1);
	if (rv == EC_SUCCESS)
		*p16 = (s->data[1] << 8) | s->data[0];
	shared_mem_release(s);
	return rv;
}
#endif

/* smbus read upto 32 bytes */
int smbus_read_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len)
{
	int rv;
	struct smbus_rd_block *s;
	rv = shared_mem_acquire(sizeof(struct smbus_rd_block), (char **)&s);
	if (rv) {
		cprintf(CC_I2C, "smbus read block[%02X] mem error\n",
			smbus_cmd);
		return rv;
	}
	s->slave_addr = slave_addr,
	s->smbus_cmd = smbus_cmd;
	s->size = MIN(len, SMBUS_MAX_BLOCK_SIZE);
	rv = smbus_if_read(i2c_port, (struct smbus_rd_if *)s, 1, s->size, 1);
	if (rv == EC_SUCCESS) {
		s->size = MIN(s->size, len);
		cprintf(CC_I2C, "smbus read block[%02X] len %d\n",
			smbus_cmd, s->size);
		memmove(data, s->data, s->size);
	}
	shared_mem_release(s);
	return rv;
}

/* smbus Read ascii string
 * Read bytestream from <slaveaddr>:<smbus_cmd> with format:
 *     [length_N] [byte_0] [byte_1] ... [byte_N-1]
 *
 * <len>      : the max length of receving buffer. to read N bytes
 *              ascii, len should be at least N+1 to include the
 *              terminating 0.
 */
int smbus_read_string(int i2c_port, uint8_t slave_addr, uint8_t smbus_cmd,
			uint8_t *data, uint8_t len)
{
	int rv;
	struct smbus_rd_block *s;
	uint8_t n, block_length, pec;

	rv = shared_mem_acquire(sizeof(struct smbus_rd_block), (char **)&s);
	if (rv) {
		cprintf(CC_I2C, "smbus read string[%02X] memory error:%d\n",
			s->smbus_cmd, rv);
		data[0] = 0;
		return rv;
	}

	s->slave_addr = slave_addr;
	s->smbus_cmd = smbus_cmd;

	/*
	 * Send device smbus_cmd, and read back block length.
	 * Keep this session open without a stop.
	 */
	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, slave_addr, &smbus_cmd, 1, &block_length, 1,
		      I2C_XFER_START);
	if (rv) {
		cprintf(CC_I2C, "smbus read string[%02X] len error:%d\n",
			s->smbus_cmd, rv);
		i2c_lock(i2c_port, 0);
		data[0] = 0;
		goto exit;
	}
	s->size = block_length;

	rv = i2c_xfer(i2c_port, slave_addr, 0, 0,
			s->data, block_length + 1 /*pec*/, I2C_XFER_STOP);
	i2c_lock(i2c_port, 0);
	if (rv) {
		cprintf(CC_I2C, "smbus read string[%02X] data error:%d\n",
			s->smbus_cmd, rv);
		data[0] = 0;
		goto exit;
	}

	s->slave_addr_rd = slave_addr | 0x01;
	n = 1 /*size_length*/ + block_length + 1 /*pec_length*/;
	pec = crc8(&s->slave_addr, n - 1 + sizeof(struct smbus_rd_if));
	if (pec != s->data[block_length]) {
		cprintf(CC_I2C, "smbus read block[%02X] PEC %02X != %02X\n",
			s->smbus_cmd, s->data[block_length], pec);
		rv = EC_ERROR_PEC;
		goto exit;
	}

	if (len && block_length > (len - 1))
		block_length = len - 1;

	memmove(data, s->data, block_length);
	data[block_length] = 0;
exit:
	shared_mem_release(s);
	return rv;
}
