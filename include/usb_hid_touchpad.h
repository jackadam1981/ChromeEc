/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB HID definitions.
 */

#ifndef __CROS_EC_USB_HID_TOUCHPAD_H
#define __CROS_EC_USB_HID_TOUCHPAD_H

#include <stdint.h>

#define USB_HID_TOUCHPAD_TIMESTAMP_UNIT 100 /* usec */

#define REPORT_ID_TOUCHPAD		0x01
#define REPORT_ID_DEVICE_CAPS		0x0A
#define REPORT_ID_DEVICE_CERT		0x0B

#define MAX_FINGERS			5

struct usb_hid_touchpad_report {
	uint8_t id; /* 0x01 */
	struct {
		uint16_t confidence:1;
		uint16_t tip:1;
		uint16_t inrange:1;
		uint16_t id:4;
		uint16_t pressure:9;
		unsigned width:12;
		unsigned height:12;
		unsigned x:12;
		unsigned y:12;
	} __packed finger[MAX_FINGERS];
	uint8_t count:7;
	uint8_t button:1;
	uint16_t timestamp;
} __packed;

/* class implementation interfaces */
void set_touchpad_report(struct usb_hid_touchpad_report *report);

#endif
