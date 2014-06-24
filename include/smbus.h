/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * smbus interface function
 *  ref:  http://smbus.org/specs/smbus20.pdf
 */
#ifndef __EC_SMBUS_H__
#define __EC_SMBUS_H__

/* Maximum transfer of a SMBUS block transfer */
#define SMBUS_MAX_BLOCK_SIZE 32

/* smbus write interface */
struct smbus_wr_if {
	uint8_t  slave_addr;/* i2c slave address */
	uint8_t  smbus_cmd; /* smbus cmd */
	uint8_t  data[0];
} __packed;

/* smbus read interface */
struct smbus_rd_if {
	uint8_t slave_addr;/* i2c wr address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t slave_addr_rd;/* i2c rd address = wr_addr | 0x1 */
	uint8_t data[0];
} __packed;

/* smbus write 2 byte + 1 pec */
struct smbus_wr_word {
	uint8_t slave_addr;/* i2c slave address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t data[3];
} __packed;

/* smbus write 1 byte size + 32 byte data + 1 byte pec */
struct smbus_wr_block {
	uint8_t slave_addr;/* i2c slave address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t size;
	uint8_t data[SMBUS_MAX_BLOCK_SIZE+1];
} __packed;

/* smbus read 2 byte + 1 pec */
struct smbus_rd_word {
	uint8_t slave_addr;/* i2c slave address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t slave_addr_rd;/* i2c rd address = wr_addr | 0x1 */
	uint8_t data[3];
} __packed;

/* smbus read 1 byte size + 32 byte data + 1 byte pec */
struct smbus_rd_block {
	uint8_t slave_addr;/* i2c slave address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t slave_addr_rd;/* i2c rd address = wr_addr | 0x1 */
	uint8_t size;
	uint8_t data[SMBUS_MAX_BLOCK_SIZE+1];
} __packed;

/* smbus read 1 byte size + 8 byte data + 1 byte pec */
struct smbus_rd_block8 {
	uint8_t slave_addr;/* i2c slave address */
	uint8_t smbus_cmd; /* smbus cmd */
	uint8_t slave_addr_rd;/* i2c rd address = wr_addr | 0x1 */
	uint8_t size;
	uint8_t data[8+1];
} __packed;

/* smbus interface write n bytes
 *   case 1:  n-1 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A][size][A] ...[Di][Ai]... [PEC][A][P]
 */
int smbus_if_write(int i2c_port, struct smbus_wr_if *intf,
			uint8_t size_n, uint8_t data_n, uint8_t pec_n);

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
		uint8_t size_n, uint8_t data_n, uint8_t pec_n);

/* smbus write 2 bytes */
int smbus_write_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t d16);

/* smbus write upto 32 bytes */
int smbus_write_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len);

/* smbus read 2 bytes */
int smbus_read_word(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint16_t *p16);

/* smbus read upto 32 bytes */
int smbus_read_block(uint8_t i2c_port, uint8_t slave_addr,
			uint8_t smbus_cmd, uint8_t *data, uint8_t len);

/* smbus Read ascii string
 * Read bytestream from <slaveaddr>:<smbus_cmd> with format:
 *     [length_N] [byte_0] [byte_1] ... [byte_N-1][byte_N='\0']
 *
 * <len>      : the max length of receving buffer. to read N bytes
 *              ascii, len should be at least N+1 to include the
 *              terminating 0.
 */
int smbus_read_string(int i2c_port, uint8_t slave_addr, uint8_t smbus_cmd,
			uint8_t *data, uint8_t len);

#endif /* __EC_SMBUS_H__ */
