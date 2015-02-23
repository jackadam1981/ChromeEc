/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#if 0
GPIO(VBUS_WAKE,         B, 5,  GPIO_INT_BOTH,    vbus_event)
GPIO(MASTER_I2C_INT_L,  C, 13, GPIO_INT_FALLING, tsu_event)
#endif

/* PD RX/TX */
GPIO(USB_CC1_PD,        A, 1,  GPIO_ANALOG,  NULL)
GPIO(PD_VBUS_P,         A, 5,  GPIO_ANALOG,  NULL)
GPIO(PD_VBUS_N,         A, 6,  GPIO_ANALOG,  NULL)
GPIO(PD_CLK_OUT,        B, 3,  GPIO_OUT_LOW, NULL)
GPIO(PD_TX_EN,          A, 15, GPIO_OUT_LOW, NULL)
GPIO(PD_TX_DATA,        B,  4, GPIO_OUT_LOW, NULL)

/* Power and muxes control */
GPIO(PP20000_EN,        A, 8,  GPIO_OUT_LOW,  NULL)
GPIO(PPVAR_VBUS_EN,     B, 12, GPIO_OUT_LOW,  NULL)
GPIO(SS_MUX_OE_L,       B, 13, GPIO_OUT_HIGH, NULL)
GPIO(SS_MUX_SEL,        B, 14, GPIO_OUT_LOW,  NULL)

/* Display Port/HDMI */
GPIO(HDP_UP,             A, 0,  GPIO_INPUT,   NULL)
GPIO(RFU_EN,             A, 7,  GPIO_OUT_LOW, NULL)

/* Communication Buses */
GPIO(PD_UART_TX,        A, 9,  GPIO_OUT_LOW,  NULL)
GPIO(PD_UART_RX,        A, 10, GPIO_INPUT,    NULL)
GPIO(BRIDGE_UART_TX,    B, 11, GPIO_OUT_LOW,  NULL)
GPIO(BRIDGE_UART_RX,    B, 10, GPIO_INPUT,    NULL)

/* Chip Resets */
GPIO(BRIDGE_RESET_L,    B, 0,  GPIO_OUT_HIGH, NULL)
GPIO(SPLITTER_RESET_L,  B, 1,  GPIO_OUT_HIGH, NULL)
GPIO(HUB_RESET_L,       B, 15, GPIO_OUT_HIGH, NULL)

/* Not used : no host on that bus */
GPIO(SLAVE_I2C_INT_L,   B, 2,  GPIO_ODR_HIGH, NULL)


/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(MASTER_I2C_SCL,    B, 6,  GPIO_INPUT, NULL)
GPIO(MASTER_I2C_SDA,    B, 7,  GPIO_INPUT, NULL)


/* Test points */
GPIO(TP9,               A, 13, GPIO_ODR_HIGH, NULL)
GPIO(TP11,              F, 1,  GPIO_ODR_HIGH, NULL)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)
#if 0
ALTERNATE(B, 0x0008, 0, MODULE_USB_PD, 0)            /* SPI1: SCK(PB3) */
ALTERNATE(B, 0x0200, 2, MODULE_USB_PD, 0)            /* TIM17_CH1: PB9 */
ALTERNATE(A, 0x0600, 1, MODULE_UART,   GPIO_PULL_UP) /* USART1: PA9/PA10 */
ALTERNATE(B, 0x0C00, 4, MODULE_UART,   GPIO_PULL_UP) /* USART3: PB10/PB11 */
#else
ALTERNATE(B, 0x2000, 0, MODULE_USB_PD, 0) /* SPI2: SCK(PB13) */
ALTERNATE(B, 0x0200, 2, MODULE_USB_PD, 0) /* TIM17_CH1: PB9) */
ALTERNATE(A, 0xC000, 1, MODULE_UART,   0) /* USART2: PA14/PA15 */
ALTERNATE(B, 0x0cc0, 1, MODULE_I2C,    0) /* I2C SLAVE:PB6/7 MASTER:PB10/11 */
#endif
