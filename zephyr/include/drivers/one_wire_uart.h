/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_ONE_WIRE_UART_H_
#define ZEPHYR_INCLUDE_DRIVERS_ONE_WIRE_UART_H_

#include <stdint.h>
#include <zephyr/kernel.h>

#include "timer.h"

#define ONE_WIRE_UART_MAX_PAYLOAD_SIZE 254

void one_wire_uart_send(const struct device *device, uint8_t cmd,
			const uint8_t *payload, int size);

void one_wire_uart_enable(const struct device *dev);

typedef void (*one_wire_uart_msg_received_cb_t)(uint8_t cmd, uint8_t *payload,
						int size);

void one_wire_uart_set_callback(const struct device *device,
				one_wire_uart_msg_received_cb_t msg_received);

/* Internal structures and methods below, put here for testing purpose only */

struct one_wire_uart_header {
	uint8_t magic;
	uint8_t payload_len;
	uint8_t checksum;
	uint8_t sender : 1;
	uint8_t reset : 1;
	uint8_t ack : 1;
	uint8_t msg_id : 5;
} __packed;

#define HEADER_SIZE sizeof(struct one_wire_uart_header)
BUILD_ASSERT(HEADER_SIZE == 4);

#define HEADER_MAGIC 0xEC

struct one_wire_uart_message {
	struct one_wire_uart_header header;
	uint8_t payload[ONE_WIRE_UART_MAX_PAYLOAD_SIZE + 1];
} __packed;

struct one_wire_uart_data {
	one_wire_uart_msg_received_cb_t msg_received_cb;
	int msg_id;
	int last_received_msg_id;

	/* queue for raw bytes */
	struct ring_buf *tx_ring_buf;
	struct ring_buf *rx_ring_buf;

	/* queue for processed messages */
	struct k_msgq *tx_queue;
	struct k_msgq *rx_queue;

	/* id of last ACK message from remote */
	int ack;

	/* resend caches */
	struct one_wire_uart_message resend_cache;
	bool msg_pending;
	timestamp_t last_send_time;
	int retry_count;
};

#ifdef CONFIG_ZTEST
uint8_t checksum(const uint8_t *data, int len);
void load_next_message(const struct device *dev);
void find_header(const struct device *dev);
void process_rx_fifo(const struct device *dev);
void process_packet(void);
void process_tx_irq(const struct device *dev);
void one_wire_uart_reset(const struct device *dev);
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_ONE_WIRE_UART_H_ */
