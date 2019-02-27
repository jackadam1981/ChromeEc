/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C slave interface for Chrome EC */

#ifndef __CROS_EC_I2CSLV_H
#define __CROS_EC_I2CSLV_H

#define I2C_SLAVE_ADDRA 0x52
#define I2C_SLAVE_ADDRA2 0x54
#define I2C_SLAVE_ADDRD 0x60
#define I2C_SLAVE_ADDRE 0x62
#define I2C_SLAVE_ADDRF 0x64

/* Data structure to define I2C slave port configuration. */
struct i2c_slv_port_t {
	const char *name;     /* Port name */
	uint8_t slave_adr;    /* slave address */
	uint8_t slave_adr2;   /* slave address2 */

};

extern const struct i2c_slv_port_t i2c_slv_ports[];

#endif /* __CROS_EC_I2CSLV_H */
