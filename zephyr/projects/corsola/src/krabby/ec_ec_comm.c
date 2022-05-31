/* Copyright 2022 The Chromium OS Authors. All rights reserved.
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

void process_packet(const uint8_t *header, uint8_t *data)
{
	if (header[4] == mode) {
		return;
	}

	if (cros_crc8(data, PACKET_SIZE) != header[3]) {
		return;
	}

#ifdef CONFIG_KEYBOARD_PROTOCOL_MKBP
	if (mode == MODE_LID) {
		int row = data[0], col = data[1], pressed = data[2];
		static uint8_t state[KEYBOARD_COLS_MAX] = {};

		if (pressed) {
			state[col] |= BIT(row);
		} else {
			state[col] &= ~BIT(row);
		}
		mkbp_keyboard_add(state);
		ccprintf("keyboard event: %d %d %d\n", row, col, pressed);
	}
#endif
}

void uart_handler(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);

	if (uart_irq_tx_complete(dev)) {
		uart_irq_tx_disable(dev);
	}

	if (uart_irq_tx_ready(dev) && !queue_is_empty(&tx_queue)) {
		struct queue_chunk chunk = queue_get_read_chunk(&tx_queue);
		int bytes_sent = uart_fifo_fill(uart, chunk.buffer, chunk.count);

		if (bytes_sent > 0) {
			queue_advance_head(&tx_queue, bytes_sent);
			uart_irq_tx_enable(uart);
		}
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

		while (queue_count(&rx_queue) >= HEADER_SIZE + PACKET_SIZE) {
			uint8_t header[HEADER_SIZE], data[PACKET_SIZE];

			queue_peek_units(&rx_queue, &header, 0, sizeof(header));
			if (header[0] != 55 || header[1] != 66 || header[2] != PACKET_SIZE) {
				queue_advance_head(&rx_queue, 1);
				continue;
			}

			queue_advance_head(&rx_queue, sizeof(header));
			queue_remove_units(&rx_queue, data, sizeof(data));

			process_packet(header, data);
		}
	}
}

static int ec_ec_comm_init(const struct device *unused)
{
	char oem_name[10];
	uint8_t out_size = sizeof(oem_name);

	ECREG(IT83XX_SPI_BASE + 0x23) = 1;

	cbi_get_board_info(CBI_TAG_OEM_NAME, oem_name, &out_size);
	if (strncmp(oem_name, "base", 4) == 0) {
		ccprintf("\x1b[1;31mmode = BASE\x1b[m");
		mode = MODE_BASE;
	} else if (strncmp(oem_name, "lid", 3) == 0) {
		ccprintf("\x1b[1;31mmode = LID\x1b[m");
		mode = MODE_LID;
	} else {
		mode = MODE_UNKNOWN;
	}

	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	return 0;
}
SYS_INIT(ec_ec_comm_init, APPLICATION, 1);

#ifndef CONFIG_KEYBOARD_PROTOCOL_MKBP
void keyboard_state_changed(int row, int col, int is_pressed)
{
	uint8_t msg[3] = {row, col, is_pressed};

	ccprintf("keyboard event: %d %d %d\n", row, col, is_pressed);
	if (mode == MODE_LID) {
		return;
	}
	board_uart_tx(msg, sizeof(msg));
}

void keyboard_clear_buffer(void)
{
	/* release all keys? */
}

void clear_typematic_key(void)
{
}
#endif
