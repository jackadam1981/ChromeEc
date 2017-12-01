/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(PCH_SLP_S0_L,   PIN(F, 0), GPIO_INT_BOTH,                   power_signal_interrupt)
#ifdef CONFIG_LOW_POWER_IDLE
GPIO_INT(UART1_RX,       PIN(B, 0), GPIO_INT_FALLING,                uart_deepsleep_interrupt)
#endif
GPIO_INT(RSMRST_L_PGOOD, PIN(G, 6), GPIO_INT_BOTH,                   power_signal_interrupt)
GPIO_INT(LID_OPEN,       PIN(E, 2), GPIO_INT_BOTH,                   lid_interrupt)
GPIO_INT(POWER_BUTTON_L, PIN(E, 4), GPIO_INT_BOTH,                   power_button_interrupt)
GPIO_INT(PCH_SLP_SUS_L,  PIN(G, 1), GPIO_INT_BOTH,                   power_signal_interrupt)
GPIO_INT(AC_PRESENT,     PIN(A, 7), GPIO_INT_BOTH,                   extpower_interrupt)
GPIO_INT(PMIC_DPWROK,    PIN(I, 7), GPIO_INT_BOTH,                   power_signal_interrupt)

GPIO(I2C_A_SCL,      PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,      PIN(B, 4), GPIO_INPUT)
GPIO(I2C_B_SCL,      PIN(C, 1), GPIO_INPUT)
GPIO(I2C_B_SDA,      PIN(C, 2), GPIO_INPUT)

GPIO(PCH_SCI_L,      PIN(D, 3), GPIO_ODR_HIGH)
GPIO(PCH_SMI_L,      PIN(D, 4), GPIO_ODR_HIGH)
GPIO(PCH_PWRBTN_L,   PIN(D, 0), GPIO_ODR_HIGH)
GPIO(PCH_WAKE_L,     PIN(D, 1), GPIO_ODR_HIGH)
GPIO(PCH_RSMRST_L,   PIN(C, 6), GPIO_OUT_LOW)
GPIO(PCH_RCIN_L,     PIN(B, 6), GPIO_ODR_HIGH)
GPIO(CPU_PROCHOT,    PIN(E, 5), GPIO_INPUT)

/* TODO: not checked yet */
#if 0
GPIO(PCH_PWROK,          PIN(K, 4), GPIO_OUT_LOW)
#endif
GPIO(PMIC_SLP_SUS_L,     PIN(J, 2), GPIO_INPUT)
GPIO(SYS_RESET_L,        PIN(J, 6), GPIO_INPUT)
GPIO(PWRGD_3V_EC,        PIN(K, 1), GPIO_INPUT)
GPIO(VERG_5VDUAL_PCH_EC, PIN(K, 2), GPIO_INPUT)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP)

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0)             /* I2C A SCL/SDA */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, 0)             /* I2C B SCL/SDA */
