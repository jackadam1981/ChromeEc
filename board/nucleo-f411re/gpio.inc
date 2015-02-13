/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(USER_BUTTON_L,       C, 13, GPIO_INT_BOTH, user_button_evt)

/* User LED */
GPIO(USER_LED,            A,  5, GPIO_OUT_LOW)

GPIO(BMI160_INT2_L,       C, 10, GPIO_OUT_LOW)
/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(MASTER_I2C_SCL,       B, 10, GPIO_INPUT)
GPIO(MASTER_I2C_SDA,       B,  3, GPIO_INPUT)
GPIO(SLAVE_I2C_SCL,        B,  8, GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,        B,  9, GPIO_INPUT)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(A, 0x000C, GPIO_ALT_USART, MODULE_UART,   GPIO_PULL_UP) /* USART2: PA2/PA3 */
ALTERNATE(B, 0x0400, GPIO_ALT_I2C, MODULE_I2C,    0) /* I2C MASTER:PB10 */
ALTERNATE(B, 0x0008, GPIO_ALT_I2C_23, MODULE_I2C, 0) /* I2C MASTER:PB3 */
ALTERNATE(B, 0x0200, GPIO_ALT_I2C, MODULE_I2C,    0) /* I2C SLAVE:PB9 */
ALTERNATE(B, 0x0100, GPIO_ALT_I2C, MODULE_I2C,    0) /* I2C SLAVE:PB8 */
