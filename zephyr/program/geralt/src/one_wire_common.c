/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "console.h"
#include "hooks.h"
#include "one_wire.h"
#include "queue.h"
#include "touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

K_MSGQ_DEFINE(rx_queue, sizeof(struct RoachMessage), 16, 4);
K_MSGQ_DEFINE(tx_queue, sizeof(struct RoachMessage), 16, 4);
static int ack = -1;
static int pending_ack = -1;

enum OneWireRole mode;

static uint8_t checksum(const struct RoachMessage* msg)
{
	uint8_t sum = 0;
	const uint8_t *raw = (const uint8_t*)msg;
	int size = HEADER_SIZE + msg->header.payload_len;

	for (int i = 0; i < size; i++) {
		sum += raw[i];
	}

	return (uint8_t)(-sum);
}

void board_uart_tx(enum RoachCommand cmd, const uint8_t* payload, int size)
{
	struct RoachMessage msg;
	static unsigned int msg_id = 0;


	if (size >= 256) {
		return;
	}

	msg.header = (struct RoachMsgHeader){
		.magic = 0xEC,
		.payload_len = size,
		.cmd = cmd,
		.sender = mode,
		.msg_id = msg_id++ % 64,
		.checksum = 0,
	};

	memcpy(msg.payload, payload, size);
	msg.header.checksum = checksum(&msg);

	if (k_msgq_put(&tx_queue, &msg, K_NO_WAIT) == 0) {
		uart_irq_tx_enable(uart);
	} else {
		ccprints("tx queue full!");
	}
}

static void process_packet(void);
DECLARE_DEFERRED(process_packet);

static void process_packet(void)
{
	struct RoachMessage msg;
	static int last_msg_id = -1;

	if (k_msgq_get(&rx_queue, &msg, K_NO_WAIT)) {
		return;
	}

	if (last_msg_id != msg.header.msg_id) {
		board_process_packet(&msg);
	}
	last_msg_id = msg.header.msg_id;

	if (k_msgq_num_used_get(&rx_queue)) {
		hook_call_deferred(&process_packet_data, MSEC / 2);
	}
}

static void gen_ack_response(struct RoachMessage *msg, int msg_id)
{
	msg->header = (struct RoachMsgHeader){
		.magic = 0xEC,
		.payload_len = 0,
		.cmd = ROACH_CMD_ACK,
		.sender = mode,
		.msg_id = msg_id,
		.checksum = 0,
	};

	msg->header.checksum = checksum(msg);
}

static void schedule_tx(void)
{
	uart_irq_tx_enable(uart);
}
DECLARE_DEFERRED(schedule_tx);


static void process_tx_irq(const struct device *dev)
{
	static timestamp_t resend_time = {.val = 0};
	static struct RoachMessage tx_head;
	static int tx_head_sent = -1;

	int filled = 0;

	/* ack received? */
	if (tx_head_sent >= 0 && ack == tx_head.header.msg_id && tx_head.header.cmd != ROACH_CMD_ACK) {
		ack = -1;
		tx_head_sent = -1;
	}

	/* prepare next message to sent? */
	if (tx_head_sent < 0) {
		if (pending_ack >= 0) {
			gen_ack_response(&tx_head, pending_ack);
			pending_ack = -1;
			tx_head_sent = 0;
			resend_time = get_time();
		} else if (k_msgq_get(&tx_queue, &tx_head, K_NO_WAIT) == 0) {
			pending_ack = -1;
			tx_head_sent = 0;
			resend_time = get_time();
		}
	}

	if (tx_head_sent >= 0 && uart_irq_tx_ready(dev)) {
		int msg_size = HEADER_SIZE + tx_head.header.payload_len;

		/* resend */
		if (tx_head_sent == msg_size && timestamp_expired(resend_time, NULL)) {
			tx_head_sent = 0;
		}

		if (tx_head_sent < msg_size) {
			filled = uart_fifo_fill(uart, (uint8_t*)&tx_head + tx_head_sent,
					msg_size - tx_head_sent);

			if (filled > 0) {
				tx_head_sent += filled;
			}

			if (tx_head_sent == msg_size && tx_head.header.cmd == ROACH_CMD_ACK) {
				tx_head_sent = -1;
			} else {
				resend_time.val = get_time().val + 3 * MSEC;
			}
		} else {
			hook_call_deferred(&schedule_tx_data, 3 * MSEC);
		}
	}

	if (filled == 0 && uart_irq_tx_complete(dev)) {
		uart_irq_tx_disable(dev);
	}
}

static void process_rx_irq(const struct device *dev)
{
	static union {
		struct RoachMessage msg;
		uint8_t buf[sizeof(struct RoachMessage) + 16];
	} recv_buf;
	static int tail = 0;

	while (true) {
		int bytes_read = uart_fifo_read(uart, recv_buf.buf + tail, 16);

		tail += bytes_read;
		/* search for header marker 0xEC */
		void *header_begin = memchr(&recv_buf, 0xEC, tail);
		if (!header_begin) {
			tail = 0;
		} else {
			int shift = (uint8_t*)header_begin - (uint8_t*)&recv_buf;
			memmove(&recv_buf, header_begin, tail - shift);
		}

		if (tail < HEADER_SIZE) {
			break;
		}

		int msg_size = HEADER_SIZE + recv_buf.msg.header.payload_len;

		/* too large ? */
		if (msg_size > ROACH_MAX_MESSAGE_SIZE) {
			/* erase current marker and find next one */
			recv_buf.buf[0] = 0;
			continue;
		}

		if (tail >= msg_size) {
			if (checksum(&recv_buf.msg) == 0) {
				if (recv_buf.msg.header.sender != mode) {
					uint8_t msg_id = recv_buf.msg.header.msg_id;

					if (recv_buf.msg.header.cmd == ROACH_CMD_ACK) {
						ack = msg_id;
					} else {
						k_msgq_put(&rx_queue, &recv_buf.msg, K_NO_WAIT);
						hook_call_deferred(&process_packet_data, 0);
						pending_ack = msg_id;
						uart_irq_tx_enable(uart);
					}
				}

				memmove(recv_buf.buf, recv_buf.buf + msg_size, tail - msg_size);
				tail -= msg_size;
			} else {
				/* erase current marker and find next one */
				recv_buf.buf[0] = 0;
				continue;
			}
		} else {
			break;
		}
	}
}

void uart_handler(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);

	if (uart_irq_rx_ready(dev)) {
		process_rx_irq(dev);
	}

	if (uart_irq_tx_ready(dev)) {
		process_tx_irq(dev);
	}
}

