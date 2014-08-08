/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */
GPIO(POWER_BUTTON_L, B, 5, GPIO_INT_BOTH,                power_button_interrupt)
GPIO(SOC1V8_XPSHOLD, A, 3, GPIO_INT_BOTH,                power_signal_interrupt)
GPIO(SUSPEND_L,      C, 7, GPIO_INPUT_UP | GPIO_INT_BOTH,power_signal_interrupt)
GPIO(SPI1_NSS,       A, 4, GPIO_INT_BOTH | GPIO_PULL_UP, spi_event)
GPIO(AC_PRESENT,     A, 0, GPIO_INT_BOTH,                extpower_interrupt)

/* Other inputs */
GPIO(LID_OPEN,       C, 13, GPIO_INPUT, NULL)
GPIO(WP_L,           B, 4,  GPIO_INPUT, NULL)

/* Outputs */
GPIO(AP_RESET_L,        B, 3,  GPIO_ODR_HIGH,  NULL)
GPIO(EC_INT,            B, 9,  GPIO_ODR_HIGH,  NULL)
GPIO(ENTERING_RW,       H, 0,  GPIO_OUT_LOW,   NULL)
GPIO(LED_POWER_L,       A, 2,  GPIO_OUT_HIGH,  NULL)  /* PWR_LED1 */
GPIO(PMIC_PWRON_L,      A, 12, GPIO_OUT_HIGH,  NULL)
GPIO(PMIC_RESET,        A, 15, GPIO_OUT_LOW,   NULL)
GPIO(PWR_LED0,          B, 10, GPIO_OUT_LOW,   NULL)
GPIO(EC_BL_OVERRIDE,    H, 1,  GPIO_ODR_HIGH,  NULL)
GPIO(PMIC_THERM_L,      A, 1,  GPIO_ODR_HIGH,  NULL)
GPIO(PMIC_WARM_RESET_L, C, 3,  GPIO_ODR_HIGH,  NULL)

ALTERNATE(A, 0x00f0, GPIO_ALT_SPI,   MODULE_SPI,  GPIO_DEFAULT)
ALTERNATE(A, 0x0600, GPIO_ALT_USART, MODULE_UART, GPIO_DEFAULT)
