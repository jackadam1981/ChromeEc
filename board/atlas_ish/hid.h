/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* General definitions for HID */
#ifndef __CROS_EC_HID_H
#define __CROS_EC_HID_H

#include <stdint.h>
#include "common.h"

#define HID_DESC_LENGTH		30
#define HID_BCD_VERSION		0x0100

/**
 *
 * HID Command Register command format:
 * ----------------------------------------------------------
 * |Byte\Bit|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * ----------------------------------------------------------
 * |    0   |  Reserved |Report Type|       Report ID       |
 * ----------------------------------------------------------
 * |    1   |       Reserved        |        Op Code        |
 * ----------------------------------------------------------
 * |    2   |    Data Register LSB (From HID descriptor)    |
 * ----------------------------------------------------------
 * |    3   |    Data Register MSB (From HID descriptor)    |
 * ----------------------------------------------------------
 * |    4   |                 Data Lng LSB                  |
 * ----------------------------------------------------------
 * |    5   |                 Data Lng MSB                  |
 * ----------------------------------------------------------
 * |    6   |                   Report ID                   |
 * ----------------------------------------------------------
 * |  7 ~ N |                     Data                      |
 * ----------------------------------------------------------
 * Note:
 * Byte 2 ~ N are optional depends on different commands and
 * vendor's define.
 *
 */
/* HID Command Register - Op code */
#define HID_CMDREG_OP_MASK		0x0F00
#define HID_CMDREG_OP_SHIFT		8

#define HID_CMDREG_OP_RSVD0		0
#define HID_CMDREG_OP_RESET		1
#define HID_CMDREG_OP_GETRPT		2
#define HID_CMDREG_OP_SETRPT		3
#define HID_CMDREG_OP_GETIDLE		4
#define HID_CMDREG_OP_SETIDLE		5
#define HID_CMDREG_OP_GETPROTOCOL	6
#define HID_CMDREG_OP_SETPROTOCOL	7
#define HID_CMDREG_OP_SETPWR		8
#define HID_CMDREG_OP_RSVD1		9
#define HID_CMDREG_OP_RSVD2		10
#define HID_CMDREG_OP_RSVD3		11
#define HID_CMDREG_OP_RSVD4		12
#define HID_CMDREG_OP_RSVD5		13
#define HID_CMDREG_OP_VENDORRSVD	14
#define HID_CMDREG_OP_RSVD6		15

/* HID Command Register - Report Type */
#define HID_CMDREG_RT_MASK              0x0030
#define HID_CMDREG_RT_SHIFT             4

#define HID_CMDREG_RT_RSVD		0
#define HID_CMDREG_RT_INPUT		1
#define HID_CMDREG_RT_OUTPUT		2
#define HID_CMDREG_RT_FEATURE		3

/* HID Command Register - Report ID */
#define HID_CMDREG_RID_MASK		0x000F
#define HID_CMDREG_RID_SHIFT		0

#define HID_CMDREG_RID_NONE		0
#define HID_CMDREG_RID_PWRWAKE		0
#define HID_CMDREG_RID_PWRSLEEP		1
#define HID_CMDREG_RID_INPUTMODE	3

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
