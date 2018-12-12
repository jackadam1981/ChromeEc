/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Routines for communicating with TSC */
#ifndef __CROS_EC_ISH_HID_H
#define __CROS_EC_ISH_HID_H

#include <stdint.h>
#include "common.h"

/* Max fingers to support */
#define MAX_FINGERS 5

struct finger {
	uint8_t confidence:1;
	uint8_t tip:1;
	uint8_t inrange:1;
	uint8_t id:5;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	uint8_t pressure;
	uint16_t orientation;
} __packed;

struct touch_report {
	uint8_t rid;
	uint8_t button:1;
	uint8_t count:7;
	uint16_t scan_time;
	struct finger finger[MAX_FINGERS];
} __packed;

struct mouse_report {
	uint8_t rid;
	uint8_t button1:1;
	/* Windows expects at least two button usages in a mouse report. The
	 * whole touchpad on eve is a single clickable surface, so button2
	 * isn't used. That said, we may later report a button2 event if the
	 * click is done on a lower corner of the touchpad.
	 */
	uint8_t button2:1;
	uint8_t unused:6;
	int8_t x;
	int8_t y;
} __packed;

extern struct touch_report touch_reports[2];
extern struct mouse_report mouse_reports[2];
extern int report_active_index;

/**
 * Send hid report to host based on report index.
 *
 * @param report_index	 < 0 - Send report by active index
 *                      >= 0 - Send report by input index
 *
 * @return non-zero if error occurred.
 */
int ish_hid_send_report(int report_index);

#endif
