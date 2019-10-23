/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* General definitions for HID */
#ifndef __CROS_EC_HID_H
#define __CROS_EC_HID_H

#include "common.h"
#include "stdint.h"

#define HID_DESC_LENGTH		sizeof(struct hid_descriptor)
#define HID_BCD_VERSION		0x0100

struct __packed hid_descriptor {
	uint16_t wHIDDescLength;
	uint16_t bcdVersion;
	uint16_t wReportDescLength;
	uint16_t wReportDescRegister;
	uint16_t wInputRegister;
	uint16_t wMaxInputLength;
	uint16_t wOutputRegister;
	uint16_t wMaxOutputLength;
	uint16_t wCommandRegister;
	uint16_t wDataRegister;
	uint16_t wVendorID;
	uint16_t wProductID;
	uint16_t wVersionID;
	uint32_t reserved;
};

#endif /* __CROS_EC_HID_H */
