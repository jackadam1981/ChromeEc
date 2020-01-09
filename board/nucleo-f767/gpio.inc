/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Interrupts */
GPIO_INT(FPS_INT,         PIN(D, 14), GPIO_INT_RISING, fps_event)
GPIO_INT(USER_BUTTON_L,   PIN(C, 13), GPIO_INT_RISING, user_button_evt_debounce)

/* User LED */
GPIO(LED_GREEN,           PIN(B,  0), GPIO_OUT_LOW)
GPIO(LED_BLUE,            PIN(B,  7), GPIO_OUT_LOW)
GPIO(LED_RED,             PIN(B, 14), GPIO_OUT_HIGH)

GPIO(FP_RST_ODL,          PIN(D, 15), GPIO_ODR_HIGH)
GPIO(SPI3_NSS,            PIN(A, 4),  GPIO_OUT_HIGH)

GPIO(EC_INT_L,            PIN(A, 8),  GPIO_ODR_HIGH)


/* USB pins */
GPIO(USB_FS_DM,			PIN(A, 11), GPIO_INPUT)
GPIO(USB_FS_DP,			PIN(A, 12), GPIO_INPUT)


UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

/* ALTERNATE(PIN_MASK(D, 0x0060), GPIO_ALT_USART,  MODULE_UART, GPIO_PULL_UP) */ /* USART2: PD5/PD6 */
/* SPI3 master to sensor: PB3/4/5 */
ALTERNATE(PIN_MASK(B, 0x0038), GPIO_ALT_SPI3, MODULE_SPI_MASTER,  0)


/* elee: try to enable USART3 on PD8/PD9 (AF7 for PD8/9). */
ALTERNATE(PIN_MASK(D, 0x0300), 7, MODULE_UART, GPIO_PULL_UP) /* USART3: PD8/PD9 - Uart to StLink */

/* elee: OK, if use MODULE_UART (instead of MODULE_USART) these pins ALSO toggle!  */
/* ALTERNATE(PIN_MASK(B, 0x0c00), 7, MODULE_UART, 0) */ /* USART3: PB10/11 - Uart to StLink */
/* ALTERNATE(PIN_MASK(C, 0x0c00), 7, MODULE_UART, 0) */ /* USART3: PC10/11 - Uart to StLink */


/* USB OTG FS */
ALTERNATE(PIN_MASK(A, 0x1800), 10, MODULE_USB, 0) /* DWC USB OTG: PA11/12 */

