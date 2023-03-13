/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "console.h"
#include "crc8.h"
#include "hooks.h"
#include "one_wire.h"
#include "queue.h"
#include "touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

static struct queue tx_queue = QUEUE_NULL(512, uint8_t);
K_MSGQ_DEFINE(rx_queue, sizeof(struct RoachMessage), 16, 4);

enum OneWireRole mode;

void board_uart_tx(enum RoachCommand cmd, const uint8_t* payload, int size)
{
	struct RoachMessage msg;


	if (size >= 256) {
		return;
	}

	msg.header = (struct RoachMsgHeader){
		.magic = 0xEC,
		.payload_len = size,
		.cmd = cmd,
		.sender = mode,
	};

	msg.header.crc = cros_crc8(payload, size);

	queue_add_units(&tx_queue, &msg.header, sizeof(msg.header));
	queue_add_units(&tx_queue, payload, size);

	uart_irq_tx_enable(uart);
}

static void process_packet(void)
{
	struct RoachMessage msg;

	while (1) {
		if (k_msgq_get(&rx_queue, &msg, K_NO_WAIT)) {
			break;
		}

		if (cros_crc8(msg.payload, msg.header.payload_len) != msg.header.crc) {
			continue;
		}

		board_process_packet(&msg);
	}
}
DECLARE_DEFERRED(process_packet);

void uart_handler(const struct device *dev, void *user_data)
{
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
		static union {
			struct RoachMessage msg;
			uint8_t buf[sizeof(struct RoachMessage) + 16];
		} recv_buf;
		static int tail = 0;

		while (true) {
			int bytes_read = uart_fifo_read(uart, recv_buf.buf + tail, 16);

			if (bytes_read == 0) {
				break;
			}

			tail += bytes_read;

			/* search for header marker 0xEC */
			int header_begin = 0;
			for (; header_begin < tail; ++header_begin) {
				if (recv_buf.buf[header_begin] == 0xEC) {
					break;
				}
			}

			if (header_begin != 0) {
				memmove(recv_buf.buf, recv_buf.buf + header_begin, tail - header_begin);
				tail -= header_begin;
			}

			int msg_size = HEADER_SIZE + recv_buf.msg.header.payload_len;

			if (msg_size <= 64 && tail >= HEADER_SIZE && tail >= msg_size) {
				if (recv_buf.msg.header.sender != mode) {
					k_msgq_put(&rx_queue, &recv_buf.msg, K_NO_WAIT);
				}

				memmove(recv_buf.buf, recv_buf.buf + msg_size, tail - msg_size);
				tail -= msg_size;
			}
		}

		hook_call_deferred(&process_packet_data, 0);
	}
}

