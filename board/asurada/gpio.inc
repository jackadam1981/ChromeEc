/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Wake Source interrupts */

/* Power sequencing interrupts */

/* Sensor Interrupts */

/* USB-C interrupts */

/* Volume button interrupts */

/* Other interrupts */
GPIO_INT(UART1_RX,    PIN(B, 0), GPIO_INT_FALLING | GPIO_PULL_UP,
         uart_deepsleep_interrupt) /* UART1 RX input */

/* Power Sequencing Signals */

/* MKBP event synchronization */

/* USB and USBC Signals */

/* Misc Signals */

/* I2C pins - Alternate function below configures I2C module on these pins */
GPIO(I2C_A_SCL,       PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,       PIN(B, 4), GPIO_INPUT)
GPIO(I2C_B_SCL,       PIN(C, 1), GPIO_INPUT)
GPIO(I2C_B_SDA,       PIN(C, 2), GPIO_INPUT)
GPIO(I2C_C_SCL,       PIN(F, 6), GPIO_INPUT)
GPIO(I2C_C_SDA,       PIN(F, 7), GPIO_INPUT)
GPIO(I2C_E_SCL,       PIN(E, 0), GPIO_INPUT)
GPIO(I2C_E_SDA,       PIN(E, 7), GPIO_INPUT)
GPIO(I2C_F_SCL,       PIN(A, 4), GPIO_INPUT)
GPIO(I2C_F_SDA,       PIN(A, 5), GPIO_INPUT)

/* SPI pins - Alternate function below configures SPI module on these pins */

/* NC / TP */

/* Keyboard pins */

/* Alternate functions GPIO definitions */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0)    /* I2C A */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, 0)    /* I2C B */
#if 0  /* temporary disabled when we are still developing on it8xxx2_evb */
ALTERNATE(PIN_MASK(F, 0xC0), 1, MODULE_I2C, 0)    /* I2C C */
#endif
ALTERNATE(PIN_MASK(E, 0x81), 1, MODULE_I2C, 0)    /* I2C E */
ALTERNATE(PIN_MASK(A, 0x30), 1, MODULE_I2C, 0)    /* I2C F, reserved */

/* UART */
ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART from EC to Servo */

/* PWM */
ALTERNATE(PIN_MASK(A, 0x0F), 1, MODULE_PWM, 0)    /* PWM 0~3 */

/* ADC */
ALTERNATE(PIN_MASK(I, 0xEF), 0, MODULE_ADC, 0)		/* ADC 0,1,2,3,5,6,7 */

/* Power Switch Logic (PSL) inputs */

/* Unimplemented Pins  */
UNIMPLEMENTED(PCH_SMI_L)
UNIMPLEMENTED(PCH_SCI_L)
UNIMPLEMENTED(PCH_WAKE_L)
UNIMPLEMENTED(PCH_PLTRST_L)

UNIMPLEMENTED(POWER_BUTTON_L)
UNIMPLEMENTED(LID_OPEN)
UNIMPLEMENTED(WP_L)

UNIMPLEMENTED(ENTERING_RW)
