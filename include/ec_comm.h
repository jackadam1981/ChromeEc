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
#include "compile_time_macros.h"

#define EC_COMM_DIR_OUT 0 /* Lid to base */
#define EC_COMM_DIR_IN 1  /* Base to lid */

#define EC_COMM_MAX_SEQ 4

#define EC_COMM_VERSION 0

enum ec_comm_commands {
	EC_COMM_BATTERY_STATIC_INFO = 0x10,
	EC_COMM_BATTERY_DYNAMIC_INFO = 0x11,
	EC_COMM_CHARGER_CONTROL = 0x20,
};

struct ec_comm_header {
	uint8_t direction:1;
	uint8_t seq:2; /* Sequence number */
	uint8_t version:3; /* Protocol version, set to EC_COMM_VERSION. */
	uint8_t _reserved1:2;
	uint8_t cmd; /* One of ec_comm_commands. */
	uint8_t _reserved2;
	uint8_t length; /* Payload length, in words (32-bit). */
} __packed;

#define EC_COMM_TEXT_MAX 8

/* Information that does not change often (including battery). */
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

/* Information that changes frequently. */
struct ec_comm_battery_dynamic_info {
	int16_t voltage; /* Battery voltage (mV) */
	int16_t current; /* Battery current (mA); negative=discharging */
	int16_t remaining_capacity; /* Remaining capacity in mAh */
	int16_t full_capacity; /* Capacity in mAh (might change occasionally) */
	int16_t status; /* Battery status */
	int16_t flags; /* Flags */
	int16_t desired_voltage; /* Charging voltage desired by battery (mV) */
	int16_t desired_current; /* Charging current desired by battery (mA) */
} __packed;
BUILD_ASSERT((sizeof(struct ec_comm_battery_dynamic_info) % 4) == 0);

struct ec_comm_charger_control {
	/* Allow battery charging (only makes sense if max_current > 0). */
	int16_t allow_charging:1;

	/*
	 * Charger current (mA). Positive to allow base to draw up to
	 * max_current and (possibly) charge battery, negative to request
	 * current from base (OTG).
	 */
	int16_t max_current:15;

	/* Voltage (mV) to use in OTG mode, ignored if max_current is >= 0. */
	uint16_t otg_voltage;

	/* FIXME: Add field to forbid battery charging. */
} __packed;
BUILD_ASSERT((sizeof(struct ec_comm_charger_control) % 4) == 0);

extern struct ec_comm_battery_static_info base_battery_static;
extern struct ec_comm_battery_dynamic_info base_battery_dynamic;

/*
 * Packed TX/RX structures for the commands above, used by the lid.
 * This is useful as the bus is half-duplex, so the master first reads back
 * its own data before reading the reply from the slave.
 */
struct ec_comm_battery_static_info_tx_rx {
	struct {
		uint32_t head;
		uint32_t crc32;
	} tx;
	struct {
		uint32_t head;
		struct ec_comm_battery_static_info info;
		uint32_t crc32;
	} rx;
} __packed;

struct ec_comm_battery_dynamic_info_tx_rx {
	struct {
		uint32_t head;
		uint32_t crc32;
	} tx;
	struct {
		uint32_t head;
		struct ec_comm_battery_dynamic_info info;
		uint32_t crc32;
	} rx;
} __packed;

struct ec_comm_charger_control_tx_rx {
	struct {
		uint32_t head;
		struct ec_comm_charger_control ctrl;
		uint32_t crc32;
	} tx;
	struct {
		uint32_t head;
		struct ec_comm_charger_control ctrl;
		uint32_t crc32;
	} rx;
} __packed;

#endif /* EC_COMM_H_ */
