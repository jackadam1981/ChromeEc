/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* TODO: for support GPIO_INT_BOTH flag, we need to choose groups 7, 10, and 12 */
/*
 * GPIO_INT(PCH_SLP_S0_L,   PIN(A, 6), GPIO_INT_BOTH, power_signal_interrupt)
 * GPIO_INT(RSMRST_L_PGOOD, PIN(C, 0), GPIO_INT_BOTH, power_signal_interrupt)
 * GPIO_INT(PCH_SLP_SUS_L,  PIN(J, 0), GPIO_INT_BOTH, power_signal_interrupt)
 * GPIO_INT(AC_PRESENT,     PIN(J, 5), GPIO_INT_BOTH, extpower_interrupt)
 * GPIO_INT(LID_OPEN,       PIN(D, 0), GPIO_INT_BOTH | GPIO_PULL_UP,   lid_interrupt)
 */

GPIO_INT(PCH_SLP_S0_L,   PIN(A, 6), GPIO_INT_RISING,                 power_signal_interrupt)
#ifdef CONFIG_LOW_POWER_IDLE
GPIO_INT(UART1_RX,       PIN(B, 0), GPIO_INT_FALLING,                uart_deepsleep_interrupt)
#endif
GPIO_INT(RSMRST_L_PGOOD, PIN(C, 0), GPIO_INT_RISING,                 power_signal_interrupt)
GPIO_INT(LID_OPEN,       PIN(D, 0), GPIO_INT_RISING  | GPIO_PULL_UP, lid_interrupt)
GPIO_INT(POWER_BUTTON_L, PIN(E, 4), GPIO_INT_BOTH    | GPIO_PULL_UP, power_button_interrupt)
/* GPIO_INT(PCH_ERST_L,     PIN(E, 6), GPIO_INT_FALLING | GPIO_PULL_UP, lpcrst_interrupt) */
GPIO_INT(PCH_SLP_SUS_L,  PIN(J, 0), GPIO_INT_RISING,                 power_signal_interrupt)
GPIO_INT(AC_PRESENT,     PIN(J, 5), GPIO_INT_RISING  | GPIO_PULL_UP, extpower_interrupt)
GPIO_INT(PMIC_DPWROK,    PIN(I, 7), GPIO_INT_BOTH,                   power_signal_interrupt)

GPIO(I2C_A_SCL,      PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,      PIN(B, 4), GPIO_INPUT)
GPIO(PCH_RCIN_L,     PIN(B, 5), GPIO_ODR_HIGH)
GPIO(PCH_SMI_L,      PIN(B, 6), GPIO_OUT_HIGH)
GPIO(PCH_PWRBTN_L,   PIN(B, 7), GPIO_OUTPUT)
GPIO(I2C_B_SCL,      PIN(C, 1), GPIO_INPUT)
GPIO(I2C_B_SDA,      PIN(C, 2), GPIO_INPUT)
GPIO(PCH_RSMRST_L,   PIN(C, 7), GPIO_OUT_LOW)
GPIO(PCH_SCI_L,      PIN(D, 3), GPIO_OUT_HIGH)
GPIO(CPU_PROCHOT,    PIN(E, 5), GPIO_OUT_LOW)
GPIO(PMIC_SLP_SUS_L, PIN(J, 2), GPIO_OUT_LOW)
GPIO(SYS_RESET_L,    PIN(J, 6), GPIO_ODR_HIGH)

/* TODO: not checked yet */
GPIO(PCH_WAKE_L,     PIN(F, 4),   GPIO_INPUT)
GPIO(ENABLE_BACKLIGHT, PIN(F, 5), GPIO_INPUT)

/* KSO/KSI pins can be used as GPIO input. */
GPIO(BOARD_VERSION1, PIN(KSO_H, 5), GPIO_INPUT)
GPIO(BOARD_VERSION2, PIN(KSO_H, 6), GPIO_INPUT)
GPIO(BOARD_VERSION3, PIN(KSO_H, 7), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP)

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0)             /* I2C A SCL/SDA */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, 0)             /* I2C B SCL/SDA */
