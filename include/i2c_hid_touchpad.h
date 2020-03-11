/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implementation of I2C HID for touchpads */
#ifndef __CROS_EC_I2C_HID_TOUCHPAD_H
#define __CROS_EC_I2C_HID_TOUCHPAD_H

#include "common.h"
#include "i2c_hid.h"
#include "stdbool.h"
#include "stdint.h"

/* Max fingers to support */
#define I2C_HID_TOUCHPAD_MAX_FINGERS	5

/* Struct holding a touchpad event
 *
 * The user should parse the original touchpad report, apply necessary
 * transformations and fill the result in this common struct. The touchpad is
 * assumed to implement the Linux HID MT-B protocol.
 */
struct touchpad_event {
	bool hover;			/* If hover is detected */
	uint8_t button;			/* If button is clicked */
	struct {
		uint16_t x;		/* X & Y of the contact */
		uint16_t y;
		uint16_t pressure;	/* Pressure/contact area */
		uint16_t width;		/* W & H of the contact */
		uint16_t height;
		uint16_t orientation;	/* Orientation of the contact ellipse */
		bool is_palm;		/* If the touchpad believes it is a palm
					 */
		bool valid;		/* If this slot contains valid contact
					 */
	} __packed finger[I2C_HID_TOUCHPAD_MAX_FINGERS];
} __packed;

#endif /* __CROS_EC_I2C_HID_TOUCHPAD_H */
