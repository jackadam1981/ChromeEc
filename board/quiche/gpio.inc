/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Wake Source interrupts */
/* TODO(b/148624000): Change this pin assignment once schematics are final */
GPIO_INT(EC_WP_L,              PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)

/* Power sequencing interrupts */

/* Sensor Interrupts */

/* USB-C interrupts */

/* HDMI interrupts */

/* Volume button interrupts */

/* AP Signals */

/* USB and USBC Signals */

/* Misc Signals */
UNIMPLEMENTED(EC_ENTERING_RW)

/* UART */
ALTERNATE(PIN_MASK(C, 0x0C00),      7,               MODULE_UART,     GPIO_PULL_UP) /* GPIOC 10-11:USART3_TX/RX */
ALTERNATE(PIN_MASK(B, 0x0080),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOB7: I2C1_SDA */
ALTERNATE(PIN_MASK(A, 0X8000),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOA15: I2C1_SCL*/
ALTERNATE(PIN_MASK(A, 0x0300),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOA8-9: I2C2_SDA/SCL */
ALTERNATE(PIN_MASK(C, 0x0300),      8,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOC8-9: I2C3_SDA/SCL */
ALTERNATE(PIN_MASK(A, 0x00F0),      5,               MODULE_SPI,      0) /* GPIOA4-7: SPI Signals */

