/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Declare symbolic names for all te GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first.
 */


GPIO_INT(HOST_USBC_PPC_INT_ODL,  PIN(B, 0),       GPIO_INT_BOTH,  ppc_interrupt)
GPIO_INT(BB_I2C_INT_L,           PIN(E, 15),      GPIO_INT_FALLING, mp4245_interrupt)
GPIO_INT(DDI_MST_IN_HPD,         PIN(F, 1),       GPIO_INT_BOTH, hpd_interrupt)
GPIO_INT(USBC_DP_MUX_ALERT_ODL,  PIN(D, 0),       GPIO_INT_BOTH,   tcpc_alert_event)
/*
 * TODO: Enable TCPC Interrupts
 * GPIO_INT(USBC_DP_MUX_ALERT_ODL,  PIN(B, 1),       GPIO_INT_BOTH,   tcpc_alert_event)
 * GPIO_INT(USBC_UF_MUX_ALERT_L,    PIN(C, 12),      GPIO_INT_BOTH,   tcpc_alert_event)
 * GPIO_INT(BB_I2C_INT_L,           PIN(E, 15),      GPIO_INT_BOTH,   tcpc_alert_event)
 */

/*
 * TODO: Enable USB Hub Interrupts
 * GPIO_INT(EC_HUB0_I2C_INT_L,      PIN(B, 5),       GPIO_INPUT,      tcpc_alert_event)
 * GPIO_INT(EC_HUB1_I2C_INT_L,      PIN(E, 9),       GPIO_INPUT,      tcpc_alert_event)
 * GPIO_INT(EC_HUB2_I2C_INT_L,      PIN(E, 10),      GPIO_INPUT,      tcpc_alert_event)
 */


/* Power sequencing interrupts */
GPIO(PWR_BTN,                       PIN(A, 0),       GPIO_INPUT)
GPIO(EN_AC_JACK,                    PIN(A, 1),       GPIO_OUT_LOW)
GPIO(EN_BB,                         PIN(C, 0),       GPIO_OUT_LOW)
GPIO(EN_PP3300_A,                   PIN(C, 10),      GPIO_OUT_LOW)
GPIO(EN_PP5000_A,                   PIN(C, 2),       GPIO_OUT_LOW)
GPIO(EN_PP1200_A,                   PIN(C, 3),       GPIO_OUT_LOW)
GPIO(EN_PP1100_A,                   PIN(C, 4),       GPIO_OUT_LOW)
GPIO(EN_PP1000_A,                   PIN(C, 5),       GPIO_OUT_LOW)
GPIO(EN_PP1050_A,                   PIN(C, 6),       GPIO_OUT_LOW)
GPIO(EN_PP5000_HSPORT,              PIN(C, 7),       GPIO_OUT_LOW)
GPIO(STATUS_LED1,                   PIN(A, 2),       GPIO_OUT_HIGH)
GPIO(STATUS_LED2,                   PIN(B, 12),      GPIO_OUT_HIGH)

/* USB-C interrupts */


/* DEBUG signals */

GPIO(DEBUG_GPIO1,                   PIN(B, 13),      GPIO_OUT_LOW)


/* MST Hub signals */

GPIO(MST_LP_CTL_L,                  PIN(B, 9),       GPIO_ODR_HIGH)
GPIO(MST_RST_L,                     PIN(B, 3),       GPIO_ODR_LOW)
/* GPIO(HDMI_IN_HPD,                   PIN(F, 0),      GPIO_INPUT) */
/* GPIO(DDI_MST_IN_HPD,                PIN(F, 1),      GPIO_INPUT) */


/* Display Demux signals */

GPIO(DEMUX_DUAL_DP_MODE,            PIN(D, 8),       GPIO_OUT_LOW)
GPIO(DEMUX_DP_HDMI_MODE,            PIN(D, 10),      GPIO_OUT_LOW)
GPIO(DEMUX_DUAL_DP_RESET_N,         PIN(B, 14),      GPIO_ODR_HIGH)
GPIO(DEMUX_DUAL_DP_PD_N,            PIN(D, 2),       GPIO_ODR_HIGH)
GPIO(DEMUX_DP_HDMI_PD_N,            PIN(D, 9),       GPIO_ODR_HIGH)

/* USBC Mux and Demux Signals */

GPIO(EN_DP_SINK,                    PIN(B, 2),       GPIO_OUT_LOW)
GPIO(DP_SINK_RESET,                 PIN(B, 1),       GPIO_OUT_LOW)
GPIO(USBC_DP_PD_RST_L,              PIN(C, 13),      GPIO_ODR_LOW)
GPIO(USBC_UF_RESET_L,               PIN(C, 11),      GPIO_ODR_LOW)

/* USB Hubs signals */

GPIO(EC_HUB1_RESET_L,               PIN(E, 13),      GPIO_ODR_LOW)
GPIO(EC_HUB2_RESET_L,               PIN(E, 14),      GPIO_ODR_LOW)


/* Configure as output to enable @1.5A on USBA Ports
* USB CDP enables. */

GPIO(USB3_A1_CDP_EN,                PIN(E, 7),       GPIO_OUT_LOW)
GPIO(USB3_A2_CDP_EN,                PIN(E, 8),       GPIO_OUT_LOW)
GPIO(USB3_P1_CDP_EN,                PIN(C, 1),       GPIO_OUT_LOW)
GPIO(USB3_P3_CDP_EN,                PIN(E, 11),      GPIO_OUT_LOW)
GPIO(USB3_P4_CDP_EN,                PIN(E, 12),      GPIO_OUT_LOW)

/* Write protect */
GPIO(EC_FLASH_WP_ODL,               PIN(A, 3),       GPIO_ODR_HIGH)
GPIO(EC_WP_L,                       PIN(A, 10),      GPIO_INT_BOTH)


/* SPI Bus */

GPIO(FLASH_SPI1_NSS,                PIN(A, 4),       GPIO_INT_FALLING)
GPIO(FLASH_SPI1_SCK,                PIN(A, 5),       GPIO_INT_BOTH)
GPIO(FLASH_SPI1_MISO,               PIN(A, 6),       GPIO_INT_BOTH)
GPIO(FLASH_SPI1_MOSI,               PIN(A, 7),       GPIO_INT_BOTH)

/* UART Bus */

GPIO(EC_UART_TX,                    PIN(B, 11),      GPIO_INT_BOTH)
GPIO(EC_UART_RX,                    PIN(B, 10),      GPIO_INT_BOTH)


/* misc signals */

GPIO(EC_I2C1_SCL,                   PIN(A, 15),      GPIO_ODR_HIGH) /* I2C port 1 SCL */
GPIO(EC_I2C1_SDA,                   PIN(B, 7),       GPIO_ODR_HIGH) /* I2C port 1 SDA */

GPIO(EC_I2C2_SDA,                   PIN(A, 8),       GPIO_ODR_HIGH) /* I2C port 2 SDA */
GPIO(EC_I2C2_SCL,                   PIN(A, 9),       GPIO_ODR_HIGH) /* I2C port 2 SCL */

GPIO(EC_I2C3_SCL,                   PIN(C, 8),       GPIO_ODR_HIGH) /* I2C port 3 SCL */
GPIO(EC_I2C3_SDA,                   PIN(C, 9),       GPIO_ODR_HIGH) /* I2C port 3 SDA */

/* Unimplemented signals since we are not an EC */
UNIMPLEMENTED(ENTERING_RW)


ALTERNATE(PIN_MASK(C, 0x0C00), GPIO_ALT_USART,       MODULE_UART,     GPIO_PULL_UP) /* GPIOC 10-11:USART3_TX/RX */
ALTERNATE(PIN_MASK(B, 0x0080),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOB7: I2C1_SDA */
ALTERNATE(PIN_MASK(A, 0X8000),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOA15: I2C1_SCL*/
ALTERNATE(PIN_MASK(A, 0x0300),      4,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOA8-9: I2C2_SDA/SCL */
ALTERNATE(PIN_MASK(C, 0x0300),      8,               MODULE_I2C,      GPIO_OPEN_DRAIN) /* GPIOC8-9: I2C3_SDA/SCL */
ALTERNATE(PIN_MASK(A, 0x00F0),      5,               MODULE_SPI,      0) /* GPIOA4-7: SPI Signals */

