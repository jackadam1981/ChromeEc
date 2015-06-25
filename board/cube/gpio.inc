/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(PCIE3_3P3_WAKE_L, PIN(A, 0),  GPIO_INT_BOTH, pcie3_wake)
GPIO_INT(PCIE0_3P3_WAKE_L, PIN(C, 13), GPIO_INT_BOTH, pcie0_wake)

/* PD, USBC */
GPIO(USB_CC1_PD,       PIN(A, 1),  GPIO_ANALOG)
GPIO(USB_CC2_PD,       PIN(A, 3),  GPIO_ANALOG)
GPIO(USB_CC1_TX_DATA,  PIN(B, 4),  GPIO_INPUT)
GPIO(USB_CC2_TX_DATA,  PIN(A, 6),  GPIO_INPUT)
GPIO(USB_CC_POLARITY,  PIN(F, 7),  GPIO_OUTPUT)
GPIO(USB_SS_MUX_EN_L,  PIN(F, 6),  GPIO_OUT_HIGH)

/* Input controls */
GPIO(USB_VBUS_DETECT,  PIN(B, 0),  GPIO_ANALOG)
GPIO(IN_CURRENT_SENSE, PIN(B, 1),  GPIO_ANALOG)
GPIO(WP_L,             PIN(B, 11), GPIO_INPUT)

/* Voltage controls */
GPIO(MCU_VDD_3R3_EN,   PIN(A, 11), GPIO_OUT_LOW)
GPIO(MCU_VDD_1R13_EN,  PIN(A, 12), GPIO_OUT_LOW)
GPIO(MCU_VDD_1R1_EN,   PIN(B, 12), GPIO_OUT_LOW)
GPIO(MCU_A380_1R35_EN, PIN(A, 8),  GPIO_OUT_LOW)
GPIO(SYS_PWR_EN,       PIN(B, 10), GPIO_OUT_HIGH)

/* Other output controls */
GPIO(ERROR_LED,        PIN(A, 13), GPIO_OUT_LOW)
/* MCU_INT_L needs to keep low for AP to reset properly. */
GPIO(MCU_INT_L,        PIN(C, 14), GPIO_OUT_LOW)

/* Unimplemented controls */
UNIMPLEMENTED(ENTERING_RW)

/*
 * I2C pins should be configured as input until I2C module is
 * initialized.
 */
GPIO(SLAVE_I2C_SCL,    PIN(B, 6),  GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,    PIN(B, 7),  GPIO_INPUT)


ALTERNATE(PIN_MASK(B, 0x0008), 0, MODULE_USB_PD, 0) /* SPI1: PB3 (CLKIN) */
ALTERNATE(PIN_MASK(B, 0x0100), 2, MODULE_USB_PD, 0) /* TIM16_CH1: PB8 (CLKOUT) */
ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART,   0) /* USART1: PA9/PA10 */
ALTERNATE(PIN_MASK(B, 0x00c0), 1, MODULE_I2C,    0) /* I2C SLAVE: PB6/PB7 */
