/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32-specific I2C module for Chrome EC */

#ifndef __CROS_EC_I2C_CHIP_H
#define __CROS_EC_I2C_CHIP_H

/* Supported I2C CLK frequencies */
enum stm32_i2c_freq {
	I2C_FREQ_1000KHZ = 0,
	I2C_FREQ_400KHZ = 1,
	I2C_FREQ_100KHZ = 2,
	I2C_FREQ_COUNT,
};

enum stm32_i2c_clk_src {
	I2C_CLK_SRC_48MHZ = 0,
	I2C_CLK_SRC_8MHZ = 1,
	I2C_CLK_SRC_COUNT,
};

#endif /* __CROS_EC_I2C_CHIP_H */
