/* -*- mode:c -*-
 *
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(EC_UP, PIN(E, 0), GPIO_INT_BOTH, button_up_event)
GPIO_INT(EC_DN, PIN(E, 1), GPIO_INT_BOTH, button_dn_event)
GPIO_INT(EC_LE, PIN(E, 2), GPIO_INT_BOTH, button_le_event)
GPIO_INT(EC_RI, PIN(E, 3), GPIO_INT_BOTH, button_ri_event)

GPIO(I2C0_SCL,            PIN(B, 2), GPIO_ODR_HIGH) /* I2C port 0 SCL */
GPIO(I2C0_SDA,            PIN(B, 3), GPIO_ODR_HIGH) /* I2C port 0 SDA */

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(A, 0x03), 1,  MODULE_UART, 0)               /* UART0 */
ALTERNATE(PIN_MASK(B, 0x04), 3,  MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SCL */
ALTERNATE(PIN_MASK(B, 0x08), 3,  MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SDA */


/* Game pad */
/*
PIO(EC_UP,         PIN(E, 0), GPIO_INPUT)
GPIO(EC_DN,         PIN(E, 1), GPIO_INPUT)
GPIO(EC_LE,         PIN(E, 2), GPIO_INPUT)
GPIO(EC_RI,         PIN(E, 3), GPIO_INPUT)
*/

/* leds */
GPIO(EC_LED0,       PIN(C, 0), GPIO_OUTPUT)
GPIO(EC_LED1,       PIN(C, 1), GPIO_OUTPUT)
GPIO(EC_LED2,       PIN(C, 2), GPIO_OUTPUT)
GPIO(EC_LED3,       PIN(C, 3), GPIO_OUTPUT)
GPIO(EC_LED4,       PIN(C, 4), GPIO_OUTPUT)
GPIO(EC_LED5,       PIN(C, 5), GPIO_OUTPUT)
GPIO(EC_LED6,       PIN(C, 6), GPIO_OUTPUT)
GPIO(EC_LED7,       PIN(C, 7), GPIO_OUTPUT)
