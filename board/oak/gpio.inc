/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */
GPIO_INT(AC_PRESENT,       C, 6,  GPIO_INT_BOTH,                extpower_interrupt)     /* AC_OK */
GPIO_INT(POWER_BUTTON_L,   B, 5,  GPIO_INT_BOTH | GPIO_PULL_UP, power_button_interrupt) /* Keyboard power button: PB 5 */
GPIO_INT(SOC_POWER_GOOD_L, A, 3,  GPIO_INT_BOTH,                power_signal_interrupt)
GPIO_INT(LID_OPEN,         C, 13, GPIO_INT_BOTH,                lid_interrupt)          /* LID switch detection */
GPIO_INT(SUSPEND_L,        C, 7,  GPIO_INT_BOTH,                power_signal_interrupt) /* AP suspend/resume state */

/* Interrupt signal from PD MCU, external pull-up */
GPIO_INT(PD_MCU_INT,     E, 0, GPIO_INT_FALLING | GPIO_INT_DSLEEP, pd_mcu_interrupt)

/* SPI Chip Select */
GPIO_INT(SPI1_NSS,       A, 4,  GPIO_INT_BOTH | GPIO_PULL_UP, spi_event)

/* Keyboard inputs */
GPIO_INT(KB_IN00, C, 8,  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN01, C, 9,  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN02, C, 10, GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN03, C, 11, GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN04, C, 12, GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN05, C, 14, GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN06, C, 15, GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)
GPIO_INT(KB_IN07, D, 2,  GPIO_KB_INPUT, keyboard_raw_gpio_interrupt)

GPIO(EC_WAKE,          A, 0,  GPIO_INPUT | GPIO_PULL_DOWN) /* wk1 */
GPIO(5V_POWER_GOOD,    A, 1,  GPIO_INPUT)
GPIO(AP_RESET_H,       B, 3,  GPIO_OUT_LOW)
GPIO(BOARD_VERSION1,   E, 10, GPIO_INPUT|GPIO_PULL_UP) /* EC_BD_ID0 */
GPIO(BOARD_VERSION2,   E, 9,  GPIO_INPUT|GPIO_PULL_UP) /* EC_BD_ID1 */
GPIO(BOARD_VERSION3,   E, 12, GPIO_INPUT) /* EC_BD_ID2 */
GPIO(BOARD_VERSION4,   E, 11, GPIO_INPUT) /* EC_BD_ID3 */

/* Outputs */
GPIO(BAT_LED0,          B, 11, GPIO_OUT_LOW)   /* LED_GREEN */
GPIO(BAT_LED1,          A, 11, GPIO_OUT_LOW)   /* LED_ORANGE */
GPIO(EC_BL_OVERRIDE,    F, 1,  GPIO_OUT_LOW)
GPIO(EC_INT,            B, 9,  GPIO_OUT_HIGH)
GPIO(ENTERING_RW,       F, 0,  GPIO_OUT_LOW)
GPIO(KB_OUT00,          B, 0,  GPIO_KB_OUTPUT)
GPIO(KB_OUT01,          B, 8,  GPIO_KB_OUTPUT)
GPIO(KB_OUT02,          B, 12, GPIO_OUT_LOW)   /* KSO2 is inverted */
GPIO(KB_OUT03,          B, 13, GPIO_KB_OUTPUT)
GPIO(KB_OUT04,          B, 14, GPIO_KB_OUTPUT)
GPIO(KB_OUT05,          B, 15, GPIO_KB_OUTPUT)
GPIO(KB_OUT06,          C, 0,  GPIO_KB_OUTPUT)
GPIO(KB_OUT07,          C, 1,  GPIO_KB_OUTPUT)
GPIO(KB_OUT08,          C, 2,  GPIO_KB_OUTPUT)
GPIO(KB_OUT09,          B, 1,  GPIO_KB_OUTPUT)
GPIO(KB_OUT10,          C, 5,  GPIO_KB_OUTPUT)
GPIO(KB_OUT11,          C, 4,  GPIO_KB_OUTPUT)
GPIO(KB_OUT12,          A, 14, GPIO_KB_OUTPUT)
GPIO(SYSTEM_POWER_H,    B, 10, GPIO_OUT_LOW)
GPIO(PMIC_PWRON_H,      A, 12, GPIO_OUT_LOW)
GPIO(PMIC_WARM_RESET_H, C, 3,  GPIO_ODR_LOW)
GPIO(WP_L,              B, 4,  GPIO_INPUT)
GPIO(LEVEL_SHIFT_EN_L,  D, 3,  GPIO_OUT_LOW)   /* LID/AC level shift */
GPIO(USB_PD_RST_L,      A, 15, GPIO_OUT_HIGH)   /* PD reset */

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C0_SCL,          B, 6,  GPIO_INPUT) /* EC I2C */
GPIO(I2C0_SDA,          B, 7,  GPIO_INPUT)
GPIO(I2C1_SCL,          B, 13, GPIO_INPUT) /* PD I2C */
GPIO(I2C1_SDA,          B, 14, GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
/* UNIMPLEMENTED(ENTERING_RW) */

ALTERNATE(A, 0x0600, 1, MODULE_UART,   0) /* USART1: PA9/PA10 */
ALTERNATE(B, 0x00c0, 1, MODULE_I2C,    0) /* I2C MASTER:PB6/7 */
ALTERNATE(A, 0x00f0, 0, MODULE_SPI,    0) /* SPI SLAVE:PA4/5/6/7 */
