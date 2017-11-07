/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */
GPIO_INT(USER_BUTTON_L, PIN(C, 13), GPIO_INT_FALLING, button_event)
GPIO_INT(LSM6DSL_INT1,  PIN(B,  5), GPIO_INT_RISING,  sensors_interrupt)
GPIO_INT(LSM6DSL_INT2,  PIN(B,  4), GPIO_INT_RISING,  sensors_interrupt)

/* Outputs */
GPIO(LED_U, PIN(A, 5), GPIO_OUT_LOW)
/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SLAVE_I2C_SCL,      PIN(B, 13), GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,      PIN(B, 14), GPIO_INPUT)
GPIO(MASTER_I2C_SCL,       PIN(B,  8), GPIO_INPUT)
GPIO(MASTER_I2C_SDA,       PIN(B,  9), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(A, 0x000C), 1, MODULE_UART,  0) /* USART2: PA2/PA3 */

/* I2C1: PB8/9*/
ALTERNATE(PIN_MASK(B, 0x0300), GPIO_ALT_F1, MODULE_I2C, GPIO_PULL_UP)
/* I2C2: PB13/14*/
ALTERNATE(PIN_MASK(B, 0x6000), GPIO_ALT_F5, MODULE_I2C, GPIO_PULL_UP)

