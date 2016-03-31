/* -*- mode:c -*-
 *
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
GPIO_INT(WP_L,               PIN(E, 1), GPIO_INT_BOTH,    switch_interrupt)         /* Write protect input */
#ifdef CONFIG_LOW_POWER_IDLE
GPIO_INT(UART1_RX,           PIN(B, 0), GPIO_INT_FALLING, uart_deepsleep_interrupt) /* UART1 RX input */
#endif

/* HVLDO disabled */
GPIO(USBPD_HVLDO,             PIN(A, 1), GPIO_OUT_LOW)
GPIO(USBPD_PORTA_CC1_VCONN,   PIN(C, 4), GPIO_OUT_LOW)
GPIO(USBPD_PORTA_CC2_VCONN,   PIN(C, 6), GPIO_OUT_LOW)
GPIO(USBPD_PORTB_CC1_VCONN,   PIN(A, 2), GPIO_OUT_LOW)
GPIO(USBPD_PORTB_CC2_VCONN,   PIN(A, 3), GPIO_OUT_LOW)
GPIO(USBPD_PORTA_VBUS_INPUT,  PIN(D, 0), GPIO_OUT_LOW)
GPIO(USBPD_PORTA_VBUS_OUTPUT, PIN(D, 1), GPIO_OUT_LOW)
GPIO(USBPD_PORTA_VBUS_DROP,   PIN(E, 5), GPIO_OUT_LOW)
GPIO(USBPD_PORTB_VBUS_INPUT,  PIN(A, 4), GPIO_OUT_LOW)
GPIO(USBPD_PORTB_VBUS_OUTPUT, PIN(A, 5), GPIO_OUT_LOW)
GPIO(USBPD_PORTB_VBUS_DROP,   PIN(A, 6), GPIO_OUT_LOW)

/* KSO/KSI pins can be used as GPIO input. */
GPIO(BOARD_VERSION1, PIN(KSO_H, 5), GPIO_INPUT)
GPIO(BOARD_VERSION2, PIN(KSO_H, 6), GPIO_INPUT)
GPIO(BOARD_VERSION3, PIN(KSO_H, 7), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
