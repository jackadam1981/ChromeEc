/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* i2c pins.  Initialize as inputs to prevent driving pins before i2c_init. */
GPIO(I2C2_SCL,	PIN(A, 24), GPIO_INPUT)
GPIO(I2C2_SDA,	PIN(A, 25), GPIO_INPUT)
GPIO(I2C3_SCL,	PIN(A, 26), GPIO_INPUT)
GPIO(I2C3_SDA,	PIN(A, 27), GPIO_INPUT)
GPIO(I2C4_SCL,	PIN(A, 28), GPIO_INPUT)
GPIO(I2C4_SDA,	PIN(A, 29), GPIO_INPUT)
GPIO(I2C5_SCL,	PIN(A, 30), GPIO_INPUT)
GPIO(I2C5_SDA,	PIN(A, 31), GPIO_INPUT)

/* UART 0 */
ALTERNATE(PIN_MASK(D, 0x00C0), 1, MODULE_UART, 0)

/* i2c ports (using bank A for now.) */
ALTERNATE(PIN_MASK(A, 0x03000000), 1, MODULE_I2C, GPIO_ODR_HIGH) /* I2C 2 */
ALTERNATE(PIN_MASK(A, 0x0C000000), 1, MODULE_I2C, GPIO_ODR_HIGH) /* I2C 3 */
ALTERNATE(PIN_MASK(A, 0x30000000), 1, MODULE_I2C, GPIO_ODR_HIGH) /* I2C 4 */
ALTERNATE(PIN_MASK(A, 0xC0000000), 1, MODULE_I2C, GPIO_ODR_HIGH) /* I2C 5 */


UNIMPLEMENTED(ENTERING_RW)
