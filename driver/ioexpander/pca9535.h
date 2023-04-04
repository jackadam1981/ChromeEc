/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PCA9535 Remote 16-BIT I2C and SMBus Low-Power I/O Expander With Interrupt
 * Output and Configuration Registers.
 */

#ifndef __CROS_EC_IOEXPANDER_PCA9535_H
#define __CROS_EC_IOEXPANDER_PCA9535_H

#define PCA9535_REG_INPUT(x) (0 + x)
#define PCA9535_REG_OUTPUT(x) (2 + x)
#define PCA9535_REG_POLARITY_INV(x) (4 + x)
#define PCA9535_REG_CONFIGURATION(x) (6 + x)

enum pca9535_io_ports { PCA9535_PORT0, PCA9535_PORT1 };

/* PCA9535 IO pins that can be referenced in gpio.inc */
enum pca9535_io_pins {
	PCA9535_IO_PIN0,
	PCA9535_IO_PIN1,
	PCA9535_IO_PIN2,
	PCA9535_IO_PIN3,
	PCA9535_IO_PIN4,
	PCA9535_IO_PIN5,
	PCA9535_IO_PIN6,
	PCA9535_IO_PIN7
};

extern const struct ioexpander_drv pca9535_ioexpander_drv;

#endif /* __CROS_EC_IOEXPANDER_PCA9535_H */
