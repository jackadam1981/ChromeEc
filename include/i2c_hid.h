* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Routines for communicating with TSC */
#ifndef __CROS_EC_I2C_HID_H
#define __CROS_EC_I2C_HID_H

#include <stdint.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

/* HID feature report. Struct maps to report descriptor feature report fields */
struct hid_accel_feature_report {
	uint8_t report_id;
	uint8_t polling_interval;
	uint8_t sensor_state;
	uint8_t power_state;
	uint16_t change_sensitivity;
	uint32_t sensor_status;
	uint32_t report_interval;
	uint8_t conn_type;
};

/* HID input report. Struct maps to report descriptor input report fields */
struct hid_accel_input_report {
	uint8_t report_id; // maps to sensor id
	uint8_t report_state;
	uint8_t event;
	uint16_t x;
	uint16_t y;
	uint16_t z;
};
