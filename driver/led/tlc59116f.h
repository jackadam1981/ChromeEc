/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_LED_TLC59116F_H
#define __CROS_EC_DRIVER_LED_TLC59116F_H

/* TLC59116F slave address */
#define TLC59116F_BASEADDR		0x60
#define TLC59116F_ALLCALL		0x68
#define TLC59116F_SUBCALL1		0x69
#define TLC59116F_SUBCALL2		0x6A
#define TLC59116F_SUBCALL3		0X6C
#define TLC59116F_RESET			0X6B

/* This depends on A0, A1, A2 ,A3. 7-bit address is 0x63. */
#define TLC59116F_I2C_ADDR_FLAG		0x63

/* TLC59116F registers */
#define TLC59116F_MODE1			0x00
#define TLC59116F_MODE2			0x01
#define TLC59116F_PWM0			0x02
#define TLC59116F_PWM1			0x03
#define TLC59116F_PWM2			0x04
#define TLC59116F_PWM3			0x05
#define TLC59116F_PWM4			0x06
#define TLC59116F_PWM5			0x07
#define TLC59116F_PWM6			0x08
#define TLC59116F_PWM7			0x09
#define TLC59116F_PWM8			0x0A
#define TLC59116F_PWM9			0x0B
#define TLC59116F_PWM10			0x0C
#define TLC59116F_PWM11			0x0D
#define TLC59116F_PWM12			0x0E
#define TLC59116F_PWM13			0x0F
#define TLC59116F_PWM14			0x10
#define TLC59116F_PWM15			0x11
#define TLC59116F_GRPPWM		0x12
#define TLC59116F_GRPFREQ		0x13
#define TLC59116F_LEDOUT0		0x14
#define TLC59116F_LEDOUT1		0x15
#define TLC59116F_LEDOUT2		0x16
#define TLC59116F_LEDOUT3		0x17
#define TLC59116F_SUBADR1		0x18
#define TLC59116F_SUBADR2		0x19
#define TLC59116F_SUBADR3		0x1A
#define TLC59116F_ALLCALLADR		0x1B

extern const struct rgbkbd_drv tlc59116f_drv;

#endif  /* __CROS_EC_DRIVER_LED_TLC59116F_H */
