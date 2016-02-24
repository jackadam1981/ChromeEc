/* -*- mode:c -*-
 *
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
GPIO_INT(WP_L,               PIN(E, 1), GPIO_INT_BOTH,                  switch_interrupt)         /* Write protect input */
#ifdef CONFIG_LOW_POWER_IDLE
GPIO_INT(UART1_RX,           PIN(B, 0), GPIO_INT_FALLING,               uart_deepsleep_interrupt) /* UART1 RX input */
#endif

/* KSO/KSI pins can be used as GPIO input. */
GPIO(BOARD_VERSION1, PIN(KSO_H, 5), GPIO_INPUT)
GPIO(BOARD_VERSION2, PIN(KSO_H, 6), GPIO_INPUT)
GPIO(BOARD_VERSION3, PIN(KSO_H, 7), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
