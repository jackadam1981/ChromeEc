/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <chip_chipregs.h>

#include "chip_chipregs.h"
#include "crc8.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "console.h"
#include "queue.h"
#include "host_command.h"
#include "keyboard_scan.h"
#include "touchpad.h"
#include "usb_hid_touchpad.h"

#define HEADER_SIZE 5
#define PACKET_SIZE 3

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));
static enum {
	MODE_UNKNOWN = 0,
	MODE_BASE,
	MODE_LID
} mode;

static struct queue tx_queue = QUEUE_NULL(512, uint8_t);
static struct queue rx_queue = QUEUE_NULL(512, uint8_t);

void board_uart_tx(const uint8_t* data, int size)
{
	uint8_t header[HEADER_SIZE] = {55, 66, size, 0 /* crc */, mode /* sender */};

	if (size >= 256) {
		return;
	}

	header[3] = cros_crc8(data, size);

	queue_add_units(&tx_queue, header, sizeof(header));
	queue_add_units(&tx_queue, data, size);

	uart_irq_tx_enable(uart);
}

void process_packet(const uint8_t *header, uint8_t *data, int data_size)
{
	if (header[4] == mode) {
		ccprintf("uart echo\n");
		return;
	}

	if (cros_crc8(data, data_size) != header[3]) {
		return;
	}

	ccprintf("control command received %02x %02x\n", data[0], data[1]);
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
				ccprintf("read %d\n", bytes_read);
				queue_add_units(&rx_queue, buf, bytes_read);
			} else {
				break;
			}
		}

		while (queue_count(&rx_queue) >= HEADER_SIZE) {
			uint8_t header[HEADER_SIZE] = {}, data[64];

			queue_peek_units(&rx_queue, &header, 0, sizeof(header));

			if (header[0] == 0) {
				++zero_count;
			} else {
				zero_count = 0;
			}
			if (mode == MODE_BASE && zero_count == 16) {
				ccprintf("\x1b[1;31mstop tx\x1b[m\n");
			}

			int data_size = header[2];
			if (header[0] != 55 || header[1] != 66 || data_size > sizeof(data)) {
				queue_advance_head(&rx_queue, 1);
				continue;
			}
			if (queue_count(&rx_queue) < HEADER_SIZE + data_size) {
				break;
			}

			queue_advance_head(&rx_queue, sizeof(header));
			queue_remove_units(&rx_queue, data, data_size);

			process_packet(header, data, data_size);
		}
	}
}

static int ec_ec_comm_init(const struct device *unused)
{
	mode = MODE_BASE;

	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	return 0;
}
SYS_INIT(ec_ec_comm_init, APPLICATION, 1);

void keyboard_state_changed(int row, int col, int is_pressed)
{
	const uint8_t *state = keyboard_scan_get_state();

	board_uart_tx(state, KEYBOARD_COLS_MAX);
}

#ifdef CONFIG_TOUCHPAD
void board_touchpad_reset(void)
{
}

void set_touchpad_report(struct usb_hid_touchpad_report *report)
{
	ccprintf("touchpad report\n");
}
#endif

