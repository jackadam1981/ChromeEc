/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Interrupts */
GPIO_INT(USB_PD_VBUS_WAKE,       C, 13,  GPIO_INT_BOTH,    vbus0_evt)
GPIO_INT(USB_C0_VBUS_WAKE_L,     C, 14,  GPIO_INT_BOTH,    vbus0_evt)
GPIO_INT(USB_C1_VBUS_WAKE_L,     C, 15,  GPIO_INT_BOTH,    vbus1_evt)
UNIMPLEMENTED(PCH_SLP_S0_L)  /* No such pin in ARM based platform */
UNIMPLEMENTED(PCH_SLP_S3_L)  /* No such pin in ARM based platform */
UNIMPLEMENTED(PCH_SLP_S5_L)  /* No such pin in ARM based platform */
UNIMPLEMENTED(WP_L)

/* PD RX/TX */
GPIO(USB_C0_CC1_PD,          A, 2,  GPIO_ANALOG)
GPIO(USB_C_REF,              A, 1,  GPIO_ANALOG)
GPIO(USB_C1_CC1_PD,          A, 0,  GPIO_ANALOG)
GPIO(USB_C0_CC2_PD,          A, 4,  GPIO_ANALOG)
GPIO(USB_C1_CC2_PD,          A, 5,  GPIO_ANALOG)

GPIO(USB_C1_CC1_TX_DATA,     B, 14, GPIO_INPUT)
GPIO(USB_C0_CC1_TX_DATA,     B, 4,  GPIO_INPUT)
GPIO(USB_C1_CC2_TX_SEL,      B, 0,  GPIO_OUT_LOW) /* C1_CC2_TX_SEL */
GPIO(USB_C0_CC2_TX_DATA,     A, 6,  GPIO_INPUT) /* C1_CCX_TX_DATA */

GPIO(PP3300_USB_PD_EN,       A, 15, GPIO_OUT_HIGH)
GPIO(USB_C0_CC1_VCONN1_EN,   B, 1,  GPIO_OUT_LOW)
GPIO(USB_C0_CC2_VCONN1_EN,   B, 2,  GPIO_OUT_LOW)
GPIO(USB_C1_CC1_VCONN1_EN,   B, 9,  GPIO_OUT_LOW)
GPIO(USB_C1_CC2_VCONN1_EN,   F, 0,  GPIO_OUT_LOW)

GPIO(USB_C0_HOST_HIGH,       A, 3,  GPIO_OUT_LOW)
GPIO(USB_C1_HOST_HIGH,       A, 7,  GPIO_OUT_LOW)
GPIO(USB_C0_CC1_ODL,         A, 11, GPIO_ODR_LOW)
GPIO(USB_C0_CC2_ODL,         A, 12, GPIO_ODR_LOW)
GPIO(USB_C1_CC1_ODL,         B, 12, GPIO_ODR_LOW)
GPIO(USB_C1_CC2_ODL,         A, 8,  GPIO_ODR_LOW)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SLAVE_I2C_SCL,          B, 6,  GPIO_INPUT)
GPIO(SLAVE_I2C_SDA,          B, 7,  GPIO_INPUT)
UNIMPLEMENTED(MASTER_I2C_SCL) /* OAK PD has only 1 I2C, in slave mode */
UNIMPLEMENTED(MASTER_I2C_SDA) /* OAK PD has only 1 I2C, in slave mode */

/* Case closed debugging. */
GPIO(EC_INT,                 A, 14, GPIO_OUT_LOW)
GPIO(ENTERING_RW,            B, 5,  GPIO_OUT_LOW)
GPIO(PD_SPI_PP3300_EN_L,     A, 15, GPIO_OUT_HIGH)
/* UNIMPLEMENTED(EC_INT) */

#if 0
/* Alternate functions */
GPIO(USB_C1_TX_CLKOUT,       B, 15, GPIO_OUT_LOW)
GPIO(USB_C0_TX_CLKOUT,       B, 8,  GPIO_OUT_LOW)
GPIO(USB_C1_TX_CLKIN,        B, 13, GPIO_OUT_LOW)
GPIO(USB_C0_TX_CLKIN,        B, 3,  GPIO_OUT_LOW)
#endif

ALTERNATE(B, 0x0008, 0, MODULE_USB_PD, 0) /* SPI1: SCK(PB3) */
ALTERNATE(B, 0x2000, 0, MODULE_USB_PD, 0) /* SPI2: SCK(PB13) */
ALTERNATE(B, 0x0100, 2, MODULE_USB_PD, 0) /* TIM16_CH1: PB8   */
ALTERNATE(B, 0x8000, 1, MODULE_USB_PD, 0) /* TIM15_CH2: PB15  */
ALTERNATE(A, 0x0600, 1, MODULE_UART,   0) /* USART1: PA9/PA10 */
ALTERNATE(B, 0x00c0, 1, MODULE_I2C,    0) /* I2C SLAVE:PB6/7  */
