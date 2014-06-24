/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * smbus interface function
 *  ref:  http://smbus.org/specs/smbus20.pdf
 */
#ifndef __EC_SMBUS_H__
#define __EC_SMBUS_H__

/* smbus interface write n bytes
 *   case 1:  n-1 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A] ...[Di][Ai]... [PEC][A][P]
 *
 *   case 2:  1 byte data-size, n -2 byte data, 1 byte PEC
 *     [S][Slave Address][Wr=0][A][cmd][A][size][A] ...[Di][Ai]... [PEC][A][P]
 */
int smbus_if_write(int i2c_port, struct ec_smbus_if *intf,
			int size_n, int data_n, int pec_n);

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
		int size_n, int data_n, int pec_n);

#endif /* __EC_SMBUS_H__ */
