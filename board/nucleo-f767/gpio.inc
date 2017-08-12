/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(USER_BUTTON_L,   PIN(C, 13), GPIO_INT_BOTH, user_button_evt)

/* User LED */
GPIO(LED_GREEN,            PIN(B,  0), GPIO_OUT_LOW)
GPIO(LED_BLUE,             PIN(B,  7), GPIO_OUT_LOW)
GPIO(LED_RED,              PIN(B, 14), GPIO_OUT_HIGH)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(D, 0x0060), GPIO_ALT_USART,  MODULE_UART, GPIO_PULL_UP) /* USART2: PD5/PD6 */
