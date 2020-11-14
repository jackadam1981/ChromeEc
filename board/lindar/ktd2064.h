/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

/* Lightbar LED header file for Lindar */

#ifndef __CROS_EC_KTD2064_H
#define __CROS_EC_KTD2064_H

/* I2C interface */
#define KTD2064_I2C_ADDR	0x68

/* define LED color */
#define KTD2064_COLOR_RED	0x03
#define KTD2064_COLOR_GREEN	0x04
#define KTD2064_COLOR_BLUE	0x05

/* define LED number */
#define KTD2064_NUM_0		0x00
#define KTD2064_NUM_1		0x09
#define KTD2064_NUM_2		0x0A
#define KTD2064_NUM_3		0x0B
#define KTD2064_NUM_4		0x0C
#define KTD2064_NUM_5		0x0D

/* define turn on/off LED */
#define LED_TURN_ON		0x88
#define LED_TURN_OFF		0x00

#define LED_INIT	0x02


/* Here is setting for LED color*/
enum lightbar_color {
	LIGHTBAR_COLOR_RED,
	LIGHTBAR_COLOR_GREEN,
	LIGHTBAR_COLOR_BLUE,
	LIGHTBAR_COLOR_CYAN,
	LIGHTBAR_COLOR_WHITE
};



#endif /* __CROS_EC_KTD2064_H */
