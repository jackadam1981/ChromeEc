/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(POWER_BUTTON_L,  PIN(0, 1), GPIO_INT_BOTH | GPIO_PULL_UP, power_button_interrupt)
GPIO_INT(VOLUME_DOWN_L,   PIN(7, 0), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(VOLUME_UP_L,     PIN(1, 1), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(WP_L,            PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)
GPIO_INT(LID_OPEN,        PIN(D, 2), GPIO_INT_BOTH, lid_interrupt)
GPIO_INT(AC_PRESENT,      PIN(0, 0), GPIO_INT_BOTH, extpower_interrupt)

GPIO(EC_SELF_RST,         PIN(E, 0), GPIO_OUT_LOW)   /* Self-reset EC */
GPIO(SYS_RESET_L,         PIN(0, 2), GPIO_ODR_HIGH)  /* Cold reset AP via PMIC */
GPIO(AP_RESET_L,          PIN(C, 1), GPIO_INPUT)     /* Cold reset AP directly */
GPIO(AP_PS_HOLD,          PIN(D, 4), GPIO_INPUT)     /* Indicate when AP triggers reset */
GPIO(SWITCHCAP_ON_L,      PIN(D, 5), GPIO_OUT_HIGH)  /* Enable switch cap */

GPIO(ENTERING_RW,         PIN(E, 1), GPIO_OUT_LOW)   /* Indicate when EC is entering RW code */
GPIO(CCD_MODE_ODL,        PIN(E, 3), GPIO_INPUT)     /* Case Closed Debug Mode */
GPIO(BAT_PRESENT_L,       PIN(E, 5), GPIO_INPUT)     /* Battery Present */
GPIO(PMIC_FAULT_L,        PIN(7, 6), GPIO_INPUT)     /* Any PMIC fault? */
GPIO(ALS_INT_L,           PIN(5, 0), GPIO_INPUT)     /* ALS sensor interrupt to EC */

/* I2C */
GPIO(I2C0_SCL,            PIN(B, 5), GPIO_INPUT)
GPIO(I2C0_SDA,            PIN(B, 4), GPIO_INPUT)
GPIO(I2C1_SCL,            PIN(9, 0), GPIO_INPUT)
GPIO(I2C1_SDA,            PIN(8, 7), GPIO_INPUT)
GPIO(I2C2_SCL,            PIN(9, 2), GPIO_INPUT)
GPIO(I2C2_SDA,            PIN(9, 1), GPIO_INPUT)
GPIO(I2C5_SCL,            PIN(3, 3), GPIO_INPUT)
GPIO(I2C5_SDA,            PIN(3, 6), GPIO_INPUT)
GPIO(I2C7_SCL,            PIN(B, 3), GPIO_INPUT)
GPIO(I2C7_SDA,            PIN(B, 2), GPIO_INPUT)

/* Board ID */
GPIO(BOARD_VERSION1,      PIN(C, 7), GPIO_INPUT)
GPIO(BOARD_VERSION2,      PIN(9, 3), GPIO_INPUT)
GPIO(BOARD_VERSION3,      PIN(8, 0), GPIO_INPUT)

/* Alternate functions GPIO definitions */
ALTERNATE(PIN_MASK(6, 0x30), 0, MODULE_UART, GPIO_PULL_UP) /* UART */
ALTERNATE(PIN_MASK(B, 0x3C), 1, MODULE_I2C, GPIO_PULL_UP)  /* I2C0 / I2C7 */
ALTERNATE(PIN_MASK(9, 0x07), 1, MODULE_I2C, GPIO_PULL_UP)  /* I2C1 SDA / I2C2 */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, GPIO_PULL_UP)  /* I2C1 SCL */
ALTERNATE(PIN_MASK(3, 0x44), 1, MODULE_I2C, GPIO_PULL_UP)  /* I2C5 */
