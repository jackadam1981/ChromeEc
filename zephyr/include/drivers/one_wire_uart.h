/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdint.h>

enum RoachCommand {
	ROACH_CMD_KEYBOARD_MATRIX,
	ROACH_CMD_TOUCHPAD_REPORT,
	ROACH_CMD_SUSPEND,
	ROACH_CMD_RESUME,
	ROACH_CMD_UPDATER_COMMAND,
};

#define ROACH_MAX_PAYLOAD_SIZE 255

void one_wire_uart_send(const struct device *device, uint8_t cmd, const uint8_t* payload, int size);

void one_wire_uart_enable(const struct device *dev);

typedef void (*one_wire_uart_msg_received_cb_t)(uint8_t cmd, uint8_t *payload, int size);

void one_wire_uart_set_callback(const struct device *device, one_wire_uart_msg_received_cb_t msg_received);
