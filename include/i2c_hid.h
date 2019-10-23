/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* General definitions for I2C HID */
#ifndef __CROS_EC_I2C_HID_H
#define __CROS_EC_I2C_HID_H

/* I2C-HID registers */
#define I2C_HID_HID_DESC_REGISTER	0x0001
#define I2C_HID_REPORT_DESC_REGISTER	0x1000
#define I2C_HID_INPUT_REPORT_REGISTER	0x2000
#define I2C_HID_COMMAND_REGISTER	0x3000
#define I2C_HID_DATA_REGISTER		0x3000

/* I2C-HID commands */
#define I2C_HID_CMD_RESET		0x01
#define I2C_HID_CMD_GET_REPORT		0x02
#define I2C_HID_CMD_SET_REPORT		0x03
#define I2C_HID_CMD_GET_IDLE		0x04
#define I2C_HID_CMD_SET_IDLE		0x05
#define I2C_HID_CMD_GET_PROTOCOL	0x06
#define I2C_HID_CMD_SET_PROTOCOL	0x07
#define I2C_HID_CMD_SET_POWER		0x08

#endif /* __CROS_EC_I2C_HID_H */
