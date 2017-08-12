/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(USER_BUTTON_L,   PIN(C, 13), GPIO_INT_BOTH, user_button_evt)
GPIO(FPS_INT,             PIN(D, 14), GPIO_INPUT)

/* User LED */
GPIO(LED_GREEN,           PIN(B,  0), GPIO_OUT_LOW)
GPIO(LED_BLUE,            PIN(B,  7), GPIO_OUT_LOW)
GPIO(LED_RED,             PIN(B, 14), GPIO_OUT_HIGH)

GPIO(FP_RST_ODL,          PIN(D, 15), GPIO_ODR_HIGH)
GPIO(SPI3_NSS,            PIN(A, 4),  GPIO_OUT_HIGH)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(D, 0x0060), GPIO_ALT_USART,  MODULE_UART, GPIO_PULL_UP) /* USART2: PD5/PD6 */
/* SPI3 master to sensor: PB3/4/5 */
ALTERNATE(PIN_MASK(B, 0x0038), GPIO_ALT_SPI3, MODULE_SPI_MASTER,  0)
