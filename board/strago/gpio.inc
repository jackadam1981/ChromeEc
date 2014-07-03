/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO(POWER_BUTTON_L,    PORT(14), 7, GPIO_INT_BOTH_DSLEEP,                      power_button_interrupt) /* Power button */
GPIO(LID_OPEN,          PORT(5),  2, GPIO_INT_BOTH_DSLEEP,                      lid_interrupt)          /* Lid switch */
GPIO(PCH_PWRBTN_L,      PORT(11), 0, GPIO_OUT_HIGH,                             NULL)                   /* Power button output to PCH */

GPIO(AC_PRESENT,        PORT(21), 1, GPIO_INT_BOTH_DSLEEP,                      extpower_interrupt)
GPIO(RSMRST_L_PGOOD,    PORT(20), 4, GPIO_INT_BOTH,                             power_signal_interrupt) /* RSMRST_N_PWRGD from power logic */
GPIO(PCH_RSMRST_L,      PORT(14), 3, GPIO_OUT_LOW,                              NULL)                   /* RSMRST_N to PCH */
GPIO(PCH_SLP_S4_L,      PORT(3),  3, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIO(PCH_SLP_S3_L,      PORT(16), 0, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S3# signal from PCH */
GPIO(ALL_SYS_PGOOD,     PORT(6),  3, GPIO_INT_BOTH_DSLEEP,                      power_signal_interrupt) /* ALL_SYS_PWRGD from power logic */
GPIO(PCH_SYS_PWROK,     PORT(12), 1, GPIO_OUT_LOW,                              NULL)                   /* EC thinks everything is up and ready (DELAY_ALL_SYS_PWRGD) */

UNIMPLEMENTED(CPU_PROCHOT)
UNIMPLEMENTED(ENABLE_TOUCHPAD)
UNIMPLEMENTED(TOUCHSCREEN_RESET_L)
UNIMPLEMENTED(PCH_RCIN_L)

GPIO(PCH_SMI_L,         PORT(4),  4, GPIO_ODR_HIGH,                             NULL)                   /* SMI output */
GPIO(PCH_WAKE_L,        PORT(20), 0, GPIO_ODR_HIGH,                             NULL)                   /* PCH wake pin */

/*
 * Signals which aren't implemented on MEC1322 eval board but we'll
 * emulate anyway, to make it more convenient to debug other code.
 */
UNIMPLEMENTED(ENTERING_RW) /* EC entering RW code */

/* Alternate functions GPIO definition */
ALTERNATE(PORT(16), 0x24,     1, MODULE_UART,          0) /* UART0 */
ALTERNATE(PORT(3),  (1 << 4), 3, MODULE_PWM_FAN,       0)
ALTERNATE(PORT(14), (1 << 0), 3, MODULE_PWM_FAN,       0)
/*
 * I2C0: External pull-up resistors on EVB are too weak. Let's
 * also enable internal pull-up here.
 */
ALTERNATE(PORT(1),  0x60,     2, MODULE_I2C,           GPIO_PULL_UP)
ALTERNATE(PORT(0),  0x3f,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(10), 0xdf,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(3),  0x04,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(4),  0x0d,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(12), 0x60,     2, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(14), 0x14,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(2),  0x20,     2, MODULE_LPC,           0)
ALTERNATE(PORT(12), 0x14,     1, MODULE_SPI,           0)
ALTERNATE(PORT(6),  0x10,     1, MODULE_SPI,           0)

