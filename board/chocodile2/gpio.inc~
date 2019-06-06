/* -*- mode:c -*-
 *
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* USER JOYSTICK */
GPIO_INT(JOY_SEL,	PIN(C,  0),	GPIO_INT_RISING |
					GPIO_PULL_DOWN, joy_interrupt)
GPIO_INT(JOY_LEFT,	PIN(C,  1),	GPIO_INT_RISING |
					GPIO_PULL_DOWN, joy_interrupt)
GPIO_INT(JOY_DOWN,	PIN(C,  2),	GPIO_INT_RISING |
					GPIO_PULL_DOWN, joy_interrupt)
GPIO_INT(JOY_RIGHT,	PIN(C,  3),	GPIO_INT_RISING |
					GPIO_PULL_DOWN, joy_interrupt)
GPIO_INT(JOY_UP,	PIN(C,  4),	GPIO_INT_RISING |
					GPIO_PULL_DOWN, joy_interrupt)

/*
 * I2C:
 *     VBUS - INA230 address 0x40
 *     CC1  - INA230 address 0x41
 *     CC2  - INA230 address 0x42
 */
GPIO(I2C0_SCL,		PIN(B, 6), GPIO_ODR_HIGH) /* I2C port 0 SCL */
GPIO(I2C0_SDA,		PIN(B, 7), GPIO_ODR_HIGH) /* I2C port 0 SDA */

/* ALERTS */
GPIO(ALERT_VBUS,	PIN(C, 5), GPIO_INPUT)
GPIO(ALERT_CC1,		PIN(C, 6), GPIO_INPUT)
GPIO(ALERT_CC2,		PIN(C, 7), GPIO_INPUT)
GPIO(DOOR_SENSE,	PIN(C, 8), GPIO_INPUT)

/* USBPD */
GPIO(ENCC1,		PIN(B, 10), GPIO_OUT_LOW)
GPIO(ENCC2,		PIN(B, 11), GPIO_OUT_LOW)
GPIO(RD_CC1,		PIN(B, 12), GPIO_INPUT)
GPIO(USBPD1CC1,		PIN(A,  8), GPIO_ANALOG)
GPIO(USBPD1CC2,		PIN(B, 15), GPIO_ANALOG)

/* LEDS */
GPIO(LD4,		PIN(D,  9), GPIO_OUT_LOW)
GPIO(LD5,		PIN(D,  8), GPIO_OUT_LOW)
GPIO(LD7,		PIN(D,  5), GPIO_OUT_LOW)
GPIO(LD6,		PIN(C, 12), GPIO_OUT_LOW)

/* LCD */
GPIO(LCD_SPI_CS_L,	PIN(A, 3),  GPIO_OUT_HIGH)
GPIO(LCD_RES_L,		PIN(A, 6),  GPIO_OUT_LOW)
GPIO(LCD_DC,		PIN(A, 7),  GPIO_OUT_HIGH)

/* UART */
GPIO(MCU_UART3_TX,	PIN(B, 8), GPIO_OUT_HIGH)
GPIO(MCU_UART3_RX,	PIN(B, 9), GPIO_INPUT)
GPIO(DEBUG,		PIN(B, 2), GPIO_OUT_LOW)

/* Unimplemented signals since this is a dev board */

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

ALTERNATE(PIN_MASK(B, 0x0300), 4, MODULE_UART, 0) /* USART3: PB8/PB9 - Console */
	
/*
 * SPI MASTER: PA1, PA2
 * AF0
 */
ALTERNATE(PIN_MASK(A, 0x06), 0, MODULE_SPI_MASTER, 0)

/*
 * I2C: PB6, PB7
 * AF6
 */
ALTERNATE(PIN_MASK(B, 0x40), 6, MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SCL */
ALTERNATE(PIN_MASK(B, 0x80), 6, MODULE_I2C,  GPIO_OPEN_DRAIN) /* I2C0 SDA */
