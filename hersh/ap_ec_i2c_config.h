/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_AP_EC_I2C_CONFIG_H
#define __CROS_AP_EC_I2C_CONFIG_H

// TODO: set these properly with dtsi's, configs, and gpios
enum i2c_supported_speeds {I2C_100_KHZ=100, I2C_400_KHZ=400, I2C_1000_KHZ=1000};
enum i2c_busses {I2C_0, I2C_1, I2C_2, I2C_3}; // TODO: check this
#define I2C_SPEED 		I2C_1000_KHZ
#define I2C_VOLTAGE 		1.8
#define INTERRUPT_VOLTAGE 	1.8
#define I2C_BUS 		I2C_0

/* I2C Bus Addresses */
// TODO: set these properly
#define AP_I2C_ADDR 0x00
#define EC_I2C_ADDR 0x01

#endif /* __CROS_AP_EC_I2C_CONFIG_H */