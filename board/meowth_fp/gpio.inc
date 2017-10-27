/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(WAKEUP_BUTTON,   PIN(A, 0),  GPIO_INT_BOTH, user_button_evt)
GPIO(FPS_INT,             PIN(D, 14), GPIO_INPUT)

/* User LED */
GPIO(LED_GREEN,           PIN(F, 10), GPIO_OUT_LOW)
GPIO(LED_RED,             PIN(A,  4), GPIO_OUT_HIGH)

GPIO(FP_RST_ODL,          PIN(D, 15), GPIO_ODR_HIGH)
GPIO(SPI3_NSS,            PIN(A, 1),  GPIO_OUT_HIGH)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(B, 0xC000), GPIO_ALT_USART1,  MODULE_UART, GPIO_PULL_UP) /* USART1: PB14/PB15 */
/* SPI3 master to sensor: PB3/4/5 */
ALTERNATE(PIN_MASK(B, 0x0038), GPIO_ALT_SPI3, MODULE_SPI_MASTER,  0)
