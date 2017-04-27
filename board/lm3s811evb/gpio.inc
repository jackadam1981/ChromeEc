/* -*- mode:c -*-
 *
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO(I2C0_SCL,            PIN(B, 2), GPIO_ODR_HIGH) /* I2C port 0 SCL */
GPIO(I2C0_SDA,            PIN(B, 3), GPIO_ODR_HIGH) /* I2C port 0 SDA */

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(A, 0x03), 1,  MODULE_UART, 0)               /* UART0 */
ALTERNATE(PIN_MASK(B, 0x04), 3,  MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SCL */
ALTERNATE(PIN_MASK(B, 0x08), 3,  MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SDA */
