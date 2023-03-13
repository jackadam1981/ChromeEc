/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include "chip_chipregs.h"
#include "crc8.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "console.h"
#include "queue.h"
#include "host_command.h"
#include "keyboard_mkbp.h"
#include "touchpad.h"
#include "usb_hid_touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));
static enum {
	MODE_UNKNOWN = 0,
	MODE_BASE,
	MODE_LID
} mode;

enum RoachCommand {
	ROACH_CMD_KEYBOARD_MATRIX,
	ROACH_CMD_TOUCHPAD_REPORT,
};

struct RoachMsgHeader {
	uint8_t magic[2];
	uint8_t payload_len;
	uint8_t crc;
	uint8_t sender;
} __packed;

#define HEADER_SIZE sizeof(struct RoachMsgHeader)
_Static_assert(HEADER_SIZE == 5);

static struct queue tx_queue = QUEUE_NULL(512, uint8_t);
static struct queue rx_queue = QUEUE_NULL(512, uint8_t);

void board_uart_tx(const uint8_t* data, int size)
{
	struct RoachMsgHeader header = {
		.magic = {55, 66},
		.payload_len = size,
		.sender = mode,
	};

	if (size >= 256) {
		return;
	}

	header.crc = cros_crc8(data, size);

	queue_add_units(&tx_queue, &header, sizeof(header));
	queue_add_units(&tx_queue, data, size);

	uart_irq_tx_enable(uart);
}

void send_stop(void)
{
	static uint8_t zeros[64] = {};
	queue_add_units(&tx_queue, zeros, sizeof(zeros));
}

void process_packet(struct RoachMsgHeader header, uint8_t *data)
{
	if (header.sender == mode) {
		return;
	}

	if (cros_crc8(data, header.payload_len) != header.crc) {
		return;
	}

	if (mode == MODE_LID) {
		if (header.payload_len == KEYBOARD_COLS_MAX) {
			mkbp_keyboard_add(data);
		}
		/*
		if (data_size == sizeof(struct usb_hid_touchpad_report)) {
			mkbp_fifo_add(EC_MKBP_EVENT_TOUCHPAD, data);
		}
		*/
	}
}

void uart_handler(const struct device *dev, void *user_data)
{
	static int zero_count = 0;

	uart_irq_update(dev);

	if (queue_is_empty(&tx_queue)) {
		uart_irq_tx_disable(dev);
	} else if (uart_irq_tx_ready(dev)) {
		struct queue_chunk chunk = queue_get_read_chunk(&tx_queue);
		int bytes_sent = uart_fifo_fill(uart, chunk.buffer, chunk.count);

		if (bytes_sent > 0) {
			queue_advance_head(&tx_queue, bytes_sent);
			uart_irq_tx_enable(uart);
		}
	} else if (!uart_irq_tx_ready(dev)) {
		uart_irq_tx_enable(dev);
	}

	if (uart_irq_rx_ready(dev)) {
		while (true) {
			uint8_t buf[32];
			int bytes_read = uart_fifo_read(uart, buf, sizeof(buf));

			if (bytes_read > 0) {
				queue_add_units(&rx_queue, buf, bytes_read);
			} else {
				break;
			}
		}

		while (queue_count(&rx_queue) >= HEADER_SIZE) {
			struct RoachMsgHeader header;
			uint8_t data[64];

			queue_peek_units(&rx_queue, &header, 0, sizeof(header));

			if (((uint8_t*)&header)[0] == 0) {
				++zero_count;
			} else {
				zero_count = 0;
			}
			if (mode == MODE_BASE && zero_count == 16) {
				ccprintf("\x1b[1;31mstop tx\x1b[m\n");
			}

			if (header.magic[0] != 55 || header.magic[1] != 66 || header.payload_len > sizeof(data)) {
				queue_advance_head(&rx_queue, 1);
				continue;
			}
			if (queue_count(&rx_queue) < HEADER_SIZE + header.payload_len) {
				break;
			}

			queue_advance_head(&rx_queue, sizeof(header));
			queue_remove_units(&rx_queue, data, header.payload_len);

			process_packet(header, data);
		}
	}
}

static int ec_ec_comm_init(const struct device *unused)
{
	mode = MODE_LID;
	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	/* UART1PMR */
	*(uint8_t*)0xf03a23 = 1;
	return 0;
}
SYS_INIT(ec_ec_comm_init, APPLICATION, 1);
