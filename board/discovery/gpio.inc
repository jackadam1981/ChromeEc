/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(USER_BUTTON, PIN(A, 0), GPIO_INT_RISING, sensors_interrupt)

/* Outputs */
GPIO(LED_BLUE,    PIN(B, 6), GPIO_OUT_LOW)
GPIO(LED_GREEN,   PIN(B, 7), GPIO_OUT_LOW)
GPIO(I2C_SCL, PIN(B, 10), GPIO_PULL_UP) /* I2C port 1 SCL */
GPIO(I2C_SDA, PIN(B, 11), GPIO_PULL_UP) /* I2C port 1 SDA */

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(A, 0x0600), GPIO_ALT_USART, MODULE_UART,  0) /* USART1: PA09/PA10 */
ALTERNATE(PIN_MASK(B, 0x0C00), GPIO_ALT_I2C, MODULE_I2C,  GPIO_PULL_UP) /* I2C2: PB10/11 */
