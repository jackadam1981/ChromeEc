/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA9675PW I/O Port expander driver header
 */

#ifndef __CROS_EC_IOEXPANDER_PCA9675_H
#define __CROS_EC_IOEXPANDER_PCA9675_H

#include <stdbool.h>

#include "hooks.h"
#include "i2c.h"

#define PCA9675_IO_P00	BIT(0)
#define PCA9675_IO_P01	BIT(1)
#define PCA9675_IO_P02	BIT(2)
#define PCA9675_IO_P03	BIT(3)
#define PCA9675_IO_P04	BIT(4)
#define PCA9675_IO_P05	BIT(5)
#define PCA9675_IO_P06	BIT(6)
#define PCA9675_IO_P07	BIT(7)

#define PCA9675_IO_P10	BIT(8)
#define PCA9675_IO_P11	BIT(9)
#define PCA9675_IO_P12	BIT(10)
#define PCA9675_IO_P13	BIT(11)
#define PCA9675_IO_P14	BIT(12)
#define PCA9675_IO_P15	BIT(13)
#define PCA9675_IO_P16	BIT(14)
#define PCA9675_IO_P17	BIT(15)

/* Send 00, 06 to reset the PCA9675 to back to power up state */
#define PCA9675_RESET_SEQ_DATA 0x0600

#define HOOK_PRIO_INIT_PCA9675 (HOOK_PRIO_INIT_I2C + 1)

/*
 * Get input level. Note that this reflects the actual level on the
 * pin, even if the pin is configured as output.
 *
 * @param port            I2C port
 * @param i2c_addr_flags  I2C address
 * @param pin             Pin number
 * @param level           Pointer to get pin level
 *
 * @return EC_SUCCESS, or EC_ERROR_* on error.
 */
int pca9675_get_pin(const int port, const uint16_t i2c_addr_flags,
			uint16_t pin, bool *level);

/*
 * Update pin levels. This function has no effect if the pin is
 * configured as input.
 *
 * @param port            I2C port
 * @param i2c_addr_flags  I2C address
 * @param setpins         Pins to be set
 * @param clearpins       Pins to be cleared
 *
 * @return EC_SUCCESS, or EC_ERROR_* on error.
 */
int pca9675_update_pins(const int port, const uint16_t i2c_addr_flags,
			uint16_t setpins, uint16_t clearpins);

/*
 * Initialize the PCA9675 to power up state
 *
 * @param port            I2C port
 * @param i2c_addr_flags  I2C address
 *
 * @return EC_SUCCESS, or EC_ERROR_* on error.
 */
int pca9675_init(int port, const uint16_t i2c_addr_flags);

#endif /* __CROS_EC_IOEXPANDER_PCA9675_H */
