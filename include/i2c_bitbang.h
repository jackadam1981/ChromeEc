/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_I2C_BITBANG_H
#define __CROS_EC_I2C_BITBANG_H

#include "i2c.h"

extern const struct i2c_port_t i2c_bitbang_ports[];

const static int i2c_bitbang_ports_used = 1;

int i2c_bitbang_xfer(const struct i2c_port_t *i2c_port,
		     const uint16_t slave_addr_flags,
		     const uint8_t *out, int out_size,
		     uint8_t *in, int in_size, int flags);

extern const struct i2c_drv bitbang_drv;

#endif /* __CROS_EC_I2C_BITBANG_H */
