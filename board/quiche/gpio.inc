/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Write protect */

GPIO(EC_FLASH_WP_ODL,               PIN(A,  3), GPIO_ODR_HIGH)
GPIO(EC_WP_L,                       PIN(E, 11), GPIO_INT_BOTH)

/* Unimplemented signals since we are not an EC */
UNIMPLEMENTED(ENTERING_RW)

/* USART3_TX/RX GPIOC 10-11*/
ALTERNATE(PIN_MASK(C, 0x0C00),  7, MODULE_UART,  GPIO_PULL_UP)
/* I2C Ports
 *   I2C1: SDA/SCL -> PB7/PA15
 *   I2C2: SDA/SCL -> PA8/PA9
 *   I2C3: SDA/SCL -> PC8/PC9
 */
ALTERNATE(PIN_MASK(B, 0x0080),  4, MODULE_I2C,   GPIO_OPEN_DRAIN)
ALTERNATE(PIN_MASK(A, 0X8000),  4, MODULE_I2C,   GPIO_OPEN_DRAIN)
ALTERNATE(PIN_MASK(A, 0x0300),  4, MODULE_I2C,   GPIO_OPEN_DRAIN)
ALTERNATE(PIN_MASK(C, 0x0300),  8, MODULE_I2C,   GPIO_OPEN_DRAIN)
/* GPIOA4-7: SPI Signals */
ALTERNATE(PIN_MASK(A, 0x00F0),  5, MODULE_SPI,   0)
