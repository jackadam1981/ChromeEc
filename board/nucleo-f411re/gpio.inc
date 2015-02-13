/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO(USER_BUTTON_L,       C, 13, GPIO_INT_BOTH, user_button_evt)

/* User LED */
GPIO(USER_LED,            A,  5, GPIO_OUT_LOW, NULL)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(MASTER_I2C_SCL,      B,  8, GPIO_INPUT,    NULL)
GPIO(MASTER_I2C_SDA,      B,  9, GPIO_INPUT,    NULL)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(A, 0x000C, GPIO_ALT_USART, MODULE_UART,   GPIO_PULL_UP) /* USART2: PA2/PA3 */
ALTERNATE(B, 0x0300, GPIO_ALT_I2C, MODULE_I2C,    0)            /* I2C MASTER:PB8/9 */
