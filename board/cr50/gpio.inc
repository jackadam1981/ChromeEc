/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */

/* User Push buttons */
GPIO(SW_N,   M,  0, GPIO_INT_BOTH, button_event)
GPIO(SW_S,   M,  1, GPIO_INT_BOTH, button_event)
GPIO(SW_W,   M,  2, GPIO_INT_BOTH, button_event)
GPIO(SW_E,   M,  3, GPIO_INT_BOTH, button_event)

/* Outputs */

/* User GPIO LEDs */
GPIO(LED_2,  A,  9,  GPIO_OUT_HIGH,  NULL)
GPIO(LED_3,  A, 10,  GPIO_OUT_LOW,   NULL)
GPIO(LED_4,  A, 11,  GPIO_OUT_LOW,  NULL)
GPIO(LED_5,  A, 12,  GPIO_OUT_LOW,  NULL)
GPIO(LED_6,  A, 13,  GPIO_OUT_LOW,  NULL)
/*GPIO(LED_7,  A, 14,  GPIO_OUT_LOW,  NULL)*/

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(MASTER_I2C_SCL,       B, 0, GPIO_INPUT,    NULL)
GPIO(MASTER_I2C_SDA,       B, 1, GPIO_INPUT,    NULL)
GPIO(SLAVE_I2C_SCL,        B, 2, GPIO_INPUT,    NULL)
GPIO(SLAVE_I2C_SDA,        B, 3, GPIO_INPUT,    NULL)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)

/* Alternate functions */
ALTERNATE(A, 0x0001, PINMUX(UART0_TX), MODULE_UART,   0) /* UART0 TX: DIOA0 */
ALTERNATE(A, 0x0002, PINMUX(UART0_RX), MODULE_UART,   0) /* UART0 RX: DIOA1 */

ALTERNATE(A, 0x0004, PINMUX(SPI0_SPICLK),  MODULE_SPI_MASTER, 0) /* SPI0_SPICLK: DIOA2 */
ALTERNATE(A, 0x0008, PINMUX(SPI0_SPIMOSI), MODULE_SPI_MASTER, 0) /* SPI0_SPIMOSI:DIOA3 */
ALTERNATE(A, 0x0010, PINMUX(SPI0_SPICSB),  MODULE_SPI_MASTER, 0) /* SPI0_SPICSB: DIOA4 */
ALTERNATE(A, 0x0020, PINMUX(SPI0_SPIMISO), MODULE_SPI_MASTER, 0) /* SPI0_SPIMISO:DIOA5 */

ALTERNATE(A, 0x0040, PINMUX(SPS0_SPICLK),  MODULE_SPI, 0) /* SPS0_SPICLK: DIOA6 */
ALTERNATE(A, 0x0080, PINMUX(SPS0_SPIMOSI), MODULE_SPI, 0) /* SPS0_SPIMOSI:DIOA7 */
ALTERNATE(A, 0x0100, PINMUX(SPS0_SPICSB),  MODULE_SPI, 0) /* SPS0_SPICSB: DIOA8 */
ALTERNATE(A, 0x0200, PINMUX(SPS0_SPIMISO), MODULE_SPI, 0) /* SPS0_SPIMISO:DIOA9 */


ALTERNATE(A, 0x0020, PINMUX(SPS0_SPIMISO), MODULE_SPI,   0)      /* SPS0_SPIMISO:DIOA5 */
