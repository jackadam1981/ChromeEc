/* -*- mode:c -*-
 *
 * Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(USER_BUTTON, PIN(A, 0), GPIO_INT_FALLING | GPIO_PULL_UP, button_event)
GPIO_INT(PD_MCU_INT,  PIN(A, 1), GPIO_INT_FALLING | GPIO_PULL_UP, alert_event)

/* Outputs */
GPIO(LED_U, PIN(C, 4), GPIO_OUT_LOW)
GPIO(LED_D, PIN(C, 5), GPIO_OUT_LOW)
GPIO(LED_L, PIN(C, 6), GPIO_OUT_LOW)
GPIO(LED_R, PIN(C, 7), GPIO_OUT_LOW)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C_A_SCL,      PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,      PIN(B, 4), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP)

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0)             /* I2C A SCL/SDA */
