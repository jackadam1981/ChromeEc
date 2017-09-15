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
#include "ec_commands.h"

#if defined(CONFIG_EC_COMM_MASTER) && defined(CONFIG_EC_COMM_BATTERY)
#define CONFIG_EC_COMM_BATTERY_MASTER
#endif

#if defined(CONFIG_EC_COMM_SLAVE) && defined(CONFIG_EC_COMM_BATTERY)
#define CONFIG_EC_COMM_BATTERY_SLAVE
#endif

#define EC_COMM_VERSION 0

#define EC_COMM_DIR_OUT 0 /* Lid to base */
#define EC_COMM_DIR_IN 1  /* Base to lid */

#define EC_COMM_MAX_SEQ 4

/* TODO(b:65697962): Convert this protocol to EC Host Command Protocol V4. */
struct ec_comm_header {
	uint8_t direction:1;
	uint8_t seq:2; /* Sequence number */
	uint8_t version:3; /* Protocol version, set to EC_COMM_VERSION. */
	uint8_t _reserved1:2;
	uint16_t cmd; /* One of EC_CMD_* command. */
	uint8_t length; /* Payload length, in words (32-bit). */
} __ec_align4;

extern struct ec_comm_battery_static_info base_battery_static;
extern struct ec_comm_battery_dynamic_info base_battery_dynamic;

/*
 * Packed TX/RX structures for the commands above, used by the lid.
 * This is useful as the bus is half-duplex, so the master first reads back
 * its own data before reading the reply from the slave.
 */
struct ec_comm_battery_static_info_tx_rx {
	struct {
		struct ec_comm_header head;
		uint32_t crc32;
	} tx;
	struct {
		struct ec_comm_header head;
		struct ec_response_battery_static_info info;
		uint32_t crc32;
	} rx;
} __ec_align4;

struct ec_comm_battery_dynamic_info_tx_rx {
	struct {
		struct ec_comm_header head;
		uint32_t crc32;
	} tx;
	struct {
		struct ec_comm_header head;
		struct ec_response_battery_dynamic_info info;
		uint32_t crc32;
	} rx;
} __ec_align4;

struct ec_comm_charger_control_tx_rx {
	struct {
		struct ec_comm_header head;
		struct ec_params_charger_control ctrl;
		uint32_t crc32;
	} tx;
	struct {
		struct ec_comm_header head;
		struct ec_response_charger_control ctrl;
		uint32_t crc32;
	} rx;
} __ec_align4;

#endif /* EC_COMM_H_ */
