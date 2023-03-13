/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdint.h>

enum OneWireRole {
	MODE_UNKNOWN = 0,
	MODE_BASE,
	MODE_LID
};

enum RoachCommand {
	ROACH_CMD_ACK,
	ROACH_CMD_KEYBOARD_MATRIX,
	ROACH_CMD_TOUCHPAD_REPORT,
};

struct RoachMsgHeader {
	uint8_t magic;
	uint8_t payload_len;
	uint8_t crc;
	uint8_t cmd;
	uint8_t sender: 2;
	uint8_t msg_id: 6;
} __packed;

#define HEADER_SIZE sizeof(struct RoachMsgHeader)
_Static_assert(HEADER_SIZE == 5);

struct RoachMessage {
	struct RoachMsgHeader header;
	uint8_t payload[64 - HEADER_SIZE];
} __packed __aligned(4);

_Static_assert(sizeof(struct RoachMessage) == 64);

extern enum OneWireRole mode;

void board_uart_tx(enum RoachCommand cmd, const uint8_t* payload, int size);

void board_process_packet(const struct RoachMessage *msg);

void uart_handler(const struct device *dev, void *user_data);
