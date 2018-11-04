/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdint.h>
/* Routines for communicating with TSC */
#ifndef __CROS_EC_I2C_HID_H
#define __CROS_EC_I2C_HID_H

#ifndef __packed
#define __packed __attribute__((packed))
#endif


/* Register definition */
#define HID_DESC_REGISTER               0x0001
#define REPORT_DESC_REGISTER            0x1000
#define INPUT_REPORT_REGISTER           0x2000
#define COMMAND_REGISTER                0x3000
#define DATA_REGISTER                   0x3000

/* I2C-HID commands */
#define I2C_HID_CMD_RESET               0x01
#define I2C_HID_CMD_GET_REPORT          0x02
#define I2C_HID_CMD_SET_REPORT          0x03
#define I2C_HID_CMD_GET_IDLE            0x04
#define I2C_HID_CMD_SET_IDLE            0x05
#define I2C_HID_CMD_GET_PROTOCOL        0x06
#define I2C_HID_CMD_SET_PROTOCOL        0x07
#define I2C_HID_CMD_SET_POWER           0x08

/* HID Report Types*/
#define INPUT_REPORT_TYPE               0x01
#define OUTPUT_REPORT_TYPE              0x02
#define FEATURE_REPORT_TYPE             0x03

/* 2 bytes for length + 1 byte for report ID */
#define I2C_HID_HEADER_SIZE             3

/* Report IDs for sensor input reports. Although they map to the indices in
motion_sensors[], we can't use 0 as ID for devices with TLCs*/
#define REPORT_ID_BASE_ACCEL            0x01
#define REPORT_ID_LID_ACCEL             0x02
#define REPORT_ID_BASE_GYRO             0x03
#define REPORT_ID_BASE_MAG              0x04
#define REPORT_ID_LID_LIGHT             0x05
#define REPORT_ID_BASE_ACCEL_SENSOR_STATE            0x02

/* Report ID for feature reports. Need this in order to know which
features to set. Strictly speaking the features are not TLCs but there's
no other way of knowing how to index the feature unless the Usage Page
is part of the host request. */
#define REPORT_ID_BASE_ACCEL_SAMPLING_RATE            0x01
#define REPORT_ID_BASE_ACCEL_POWER_STATE            0x02
#define REPORT_ID_BASE_ACCEL_CHANGE_SENSITIVITY            0x03
#define REPORT_ID_BASE_ACCEL_SENSOR_STATUS            0x04
#define REPORT_ID_BASE_ACCEL_REPORT_INTERVAL            0x05
#define REPORT_ID_BASE_ACCEL_REPORTING_STATE            0x06

int i2c_hid_command_process(int len, uint8_t* buffer,
                                   void (*send_response)(int len));

struct __attribute__ ((__packed__)) hid_descriptor {
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
        uint16_t wVersionID;
        uint32_t reserved;
};

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
	uint8_t sensor_state;
	uint8_t sensor_event;
	uint16_t x;
	uint16_t y;
	uint16_t z;
};

int extract_report(uint64_t len, uint8_t* buffer, void* data,
			  uint64_t data_len);


int i2c_hid_process(int len, uint8_t* buffer,
		     void(*send_response)(int len));

#endif /* __CROS_EC_I2C_HID_H */
