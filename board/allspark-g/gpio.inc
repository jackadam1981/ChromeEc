/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PD, USBC */
GPIO(USB_TEMP_DETECT,  PIN(A, 0),  GPIO_ANALOG)
GPIO(USB_CC1_PD,       PIN(A, 1),  GPIO_ANALOG)
GPIO(USB_CC1_TX_DATA,  PIN(B, 4),  GPIO_INPUT)
GPIO(USB_VBUS_OUTPUT,  PIN(F, 0),  GPIO_OUT_LOW)

/* Input controls */
GPIO(USB_VBUS_DETECT,  PIN(A, 2),  GPIO_ANALOG)

/* Unimplemented controls */
UNIMPLEMENTED(WP_L)
UNIMPLEMENTED(ENTERING_RW)

/*
 * I2C pins should be configured as input until I2C module is
 * initialized.
 */
GPIO(MASTER_I2C_SCL,    PIN(B, 6),  GPIO_INPUT)
GPIO(MASTER_I2C_SDA,    PIN(B, 7),  GPIO_INPUT)

/* Ethernet control */
GPIO(ETHERNET_POWER_EN, PIN(A, 15),  GPIO_OUT_LOW)

/* Realtek switch controls */
GPIO(RTK_SW_VDDH_EN,    PIN(B, 0),  GPIO_OUT_LOW)
GPIO(RTK_SW_VDDL_EN,    PIN(B, 1),  GPIO_OUT_LOW)

ALTERNATE(PIN_MASK(B, 0x0008), 0, MODULE_USB_PD, 0) /* SPI1: PB3 (CLKIN) */
ALTERNATE(PIN_MASK(B, 0x0100), 2, MODULE_USB_PD, 0) /* TIM16_CH1: PB8 (CLKOUT) */
ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART,   0) /* USART1: PA9/PA10 */
ALTERNATE(PIN_MASK(B, 0x00c0), 1, MODULE_I2C,    0) /* I2C MASTER: PB6/PB7 */
