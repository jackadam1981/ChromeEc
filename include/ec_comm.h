/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EC communication protocol.
 *
 * Command length is always a multiple of 32-bit (word).
 * Basic command layout:
 *  - 1 word: Header (direction, command, <length>)
 *  - <length> words: payload
 *  - 1 word: CRC32, computed with USB3/PD definitions.
 */

#ifndef EC_COMM_H_
#define EC_COMM_H_
#include <stdint.h>
#include "battery.h"

#define EC_COMM_DIR_OUT 0xca /* Lid to base */
#define EC_COMM_DIR_IN 0x35  /* Base to lid */

enum ec_comm_commands {
	EC_COMM_BATTERY_STATIC_INFO = 10,
	EC_COMM_BATTERY_DYNAMIC_INFO = 11,
	EC_COMM_CHARGER_CONTROL = 20,
};

struct ec_comm_header {
	uint8_t direction;
	uint8_t cmd;
	uint8_t _reserved;
	uint8_t length; /* 4 bytes unit */
} __packed;

#define EC_COMM_TEXT_MAX 8

/* Battery information that does not change */
struct ec_comm_battery_static_info {
	uint16_t design_capacity; /* Battery Design Capacity */
	uint16_t design_voltage; /* Battery Design Voltage */
	uint32_t cycle_count; /* Battery Cycle Count */
	char manufacturer[EC_COMM_TEXT_MAX]; /* Battery Manufacturer String */
	char model[EC_COMM_TEXT_MAX]; /* Battery Model Number String */
	char serial[EC_COMM_TEXT_MAX]; /* Battery Serial Number String */
	char type[EC_COMM_TEXT_MAX]; /* Battery Type String */
} __packed;
BUILD_ASSERT((sizeof(struct ec_comm_battery_static_info) % 4) == 0);

struct ec_comm_battery_dynamic_info {
	/* Those parameters are mostly for presentation to AP. */
        int16_t voltage;       /* Battery voltage (mV) */
        int16_t current;       /* Battery current (mA); negative=discharging */
        int16_t remaining_capacity;  /* Remaining capacity in mAh */
        int16_t full_capacity; /* Capacity in mAh (might change occasionally) */
        int16_t status;        /* Battery status */
        int16_t flags;         /* Flags */
	/* These are used by lid EC to make power decisions. */
        int16_t desired_voltage; /* Charging voltage desired by battery (mV) */
        int16_t desired_current; /* Charging current desired by battery (mA) */
} __packed;
BUILD_ASSERT((sizeof(struct ec_comm_battery_dynamic_info) % 4) == 0);

/* FIXME: Exact command TBD */
struct ec_comm_charger_control {
	uint16_t max_current; /* mA */
	uint16_t _reserved;
} __packed;
BUILD_ASSERT((sizeof(struct ec_comm_charger_control) % 4) == 0);

#endif /* EC_COMM_H_ */
