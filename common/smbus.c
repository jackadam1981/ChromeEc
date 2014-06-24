/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * smbus cross-platform code for Chrome EC
 *  ref: http://smbus.org/specs/smbus20.pdf
 */

#include "common.h"
#include "console.h"
#include "util.h"
#include "ec_commands.h"
#include "i2c.h"
#include "smbus.h"
#include "crc.h"

/* smbus interface write n bytes
 *   case 1:  n-1 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A][size][A] ...[Di][Ai]... [PEC][A][P]
 */
int smbus_if_write(int i2c_port, struct ec_smbus_if *intf,
			int size_n, int data_n, int pec_n)
{
	int rv;
	int n = size_n + data_n + pec_n;
	if (pec_n)
		intf->data[n-1] = crc8(&(intf->slave_addr),
				n - 1 + sizeof(struct ec_smbus_if));
	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr,
			&intf->smbus_cmd, n + 1, NULL, 0, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);
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
int smbus_if_read(int i2c_port, struct ec_smbus_rd_if *intf,
		int size_n, int data_n, int pec_n)
{
	int rv;
	uint8_t pec;
	int n = size_n + data_n + pec_n;

	i2c_lock(i2c_port, 1);
	rv = i2c_xfer(i2c_port, intf->slave_addr, &intf->smbus_cmd, 1,
		intf->data, n, I2C_XFER_SINGLE);
	i2c_lock(i2c_port, 0);

	if (rv) {
		cprintf(CC_I2C, "smbus i2c_xfer error:%d\n", rv);
		return rv;
	}
	if (pec_n == 0)
		return EC_SUCCESS;

	intf->slave_addr_rd = intf->slave_addr | 0x01;
	pec = crc8(&intf->slave_addr, n - 1 + sizeof(struct ec_smbus_rd_if));
	if (pec != intf->data[n-1]) {
		cprintf(CC_I2C, "smbus read[%02X] PEC %02X != %02X\n",
			intf->smbus_cmd, intf->data[n-1], pec);
		return EC_RES_ERROR;
	}
	return EC_SUCCESS;
}
