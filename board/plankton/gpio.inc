/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */
GPIO(VBUS_WAKE,         B, 5, GPIO_INT_BOTH, vbus_event)

/* PD RX/TX */
GPIO(USBC_PD_REF,       A, 0,  GPIO_ANALOG,   NULL)
GPIO(USBC_CC1_PD,       A, 1,  GPIO_ANALOG,   NULL)
GPIO(USBC_CC_TX_EN,     A, 3,  GPIO_OUT_LOW,  NULL)
GPIO(USBC_CC_TX_DATA,   A, 6,  GPIO_OUT_LOW,  NULL)

#if 0
/* Alternate functions */
GPIO(USBC_TX_CLKOUT,    B, 9,  GPIO_OUT_LOW,  NULL)
GPIO(USBC_TX_CLKIN,     A, 4,  GPIO_OUT_LOW,  NULL)
#endif

/* USB-C Power and muxes control */
GPIO(USBC_CHARGE_EN_L,  A, 8,  GPIO_OUT_LOW,  NULL)
GPIO(USBC_CC_DEVICE_ODL,A, 9,  GPIO_ODR_LOW,  NULL)
GPIO(USBC_5V_EN,        A, 10, GPIO_OUT_LOW,  NULL)
GPIO(USBC_DP_MODE_L,    B, 0,  GPIO_OUT_LOW,  NULL)
GPIO(USBC_DP_POLARITY,  B, 1,  GPIO_OUT_HIGH, NULL)
GPIO(USBC_SS_USB_MODE,  B, 3,  GPIO_OUT_LOW,  NULL)
GPIO(USBC_SS_EN_L,      B, 4,  GPIO_OUT_HIGH, NULL)

/* Alternate functions */
#if 0
GPIO(USB_DM,            A, 11, GPIO_ANALOG,   NULL)
GPIO(USB_DP,            A, 12, GPIO_ANALOG,   NULL)
GPIO(UART_TX,           A, 14, GPIO_OUT_LOW,  NULL)
GPIO(UART_RX,           A, 15, GPIO_OUT_LOW,  NULL)
#endif

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SLAVE_I2C_SCL,     B, 6,  GPIO_INPUT,    NULL)
GPIO(SLAVE_I2C_SDA,     B, 7,  GPIO_INPUT,    NULL)

/* Outputs */
GPIO(SLAVE_I2C_INT_L,   B, 2,  GPIO_OUT_HIGH, NULL)
GPIO(RAIDEN_CC1_EN_L,   B, 13, GPIO_OUT_LOW,  NULL)
GPIO(RAIDEN_CC2_EN_L,   B, 14, GPIO_OUT_HIGH, NULL)

/* Inputs */
GPIO(DBG_12V_TO_DUT_L,  A, 4,  GPIO_INPUT,    NULL)
GPIO(DBG_CHG_TO_DEV_L,  A, 6,  GPIO_INPUT,    NULL)
GPIO(DBG_5V_TO_DUT_L,   B, 8,  GPIO_INPUT,    NULL)
GPIO(DBG_MODE_EN_L,     B, 10, GPIO_INPUT,    NULL)
GPIO(DBG_USB_EN_L,      B, 11, GPIO_INPUT,    NULL)
GPIO(DBG_DP_EN_L,       B, 12, GPIO_INPUT,    NULL)
GPIO(DBG_STATUS_CLEAR_L,C, 13, GPIO_INPUT,    NULL)
GPIO(DBG_CABLE_FLIP_L,  B, 15, GPIO_INPUT,    NULL)
GPIO(SLOT_PLN_A0,       C, 15, GPIO_INPUT,    NULL)
GPIO(SLOT_PLN_A1,       F, 0,  GPIO_INPUT,    NULL)
GPIO(SLOT_PLN_A2,       F, 1,  GPIO_INPUT,    NULL)

/* Test points */
GPIO(TP9,               A, 13, GPIO_ODR_HIGH, NULL)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)
