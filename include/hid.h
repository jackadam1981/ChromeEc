/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header file with HID protocol generic components that are shared among
 * various HID devices
 */

/* Register definition */
#define REPORT_DESC_REGISTER			0x5000
#define INPUT_REPORT_REGISTER			0x2000
#define COMMAND_REGISTER			0x3001
#define DATA_REGISTER				0x3002

/* I2C-HID commands */
#define I2C_HID_CMD_RESET			0x01
#define I2C_HID_CMD_GET_REPORT			0x02
#define I2C_HID_CMD_SET_REPORT			0x03
#define I2C_HID_CMD_GET_IDLE			0x04
#define I2C_HID_CMD_SET_IDLE			0x05
#define I2C_HID_CMD_GET_PROTOCOL		0x06
#define I2C_HID_CMD_SET_PROTOCOL		0x07
#define I2C_HID_CMD_SET_POWER			0x08

/* HID Report Types*/
#define INPUT_REPORT_TYPE			0x01
#define OUTPUT_REPORT_TYPE			0x02
#define FEATURE_REPORT_TYPE			0x03

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
	uint16_t wVendorID;
	uint16_t wProductID;
	uint16_t wVersionID;
	uint32_t reserved;
};
