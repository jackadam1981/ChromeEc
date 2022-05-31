/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <devicetree.h>
#include <kernel.h>

#include "chip_chipregs.h"
#include "crc8.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "console.h"
#include "queue.h"
#include "host_command.h"

#define HEADER_SIZE 5
#define PACKET_SIZE 32

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));
static enum {
	MODE_UNKNOWN = 0,
	MODE_BASE,
	MODE_LID
} mode;

static struct queue tx_queue = QUEUE_NULL(512, uint8_t);
static struct queue rx_queue = QUEUE_NULL(512, uint8_t);

static K_SEM_DEFINE(rx_sem, 0, 1);

void board_uart_tx(const uint8_t* data, int size)
{
	uint8_t header[HEADER_SIZE] = {55, 66, size, 0 /* crc */, mode};

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

	if (mode == MODE_BASE) {
		for (int i = 0; i < PACKET_SIZE; i++) {
			data[i] = data[i] + 1;
		}
		board_uart_tx(data, PACKET_SIZE);
	}

	if (mode == MODE_LID) {
		k_sem_give(&rx_sem);
	}
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

static int command_uart_test(int argc, char **argv)
{
	uint64_t buf[PACKET_SIZE / 8];
	uint64_t t = get_time().val;

	for (int i = 0; i < ARRAY_SIZE(buf); i++) {
		buf[i] = t;
	}
	board_uart_tx((char*)buf, sizeof(buf));

	return 0;
}
DECLARE_CONSOLE_COMMAND(a, command_uart_test, NULL, "");

static enum ec_status cmd_touchpad_self_test(struct host_cmd_handler_args *args)
{
	args->response_size = 0;

	k_sem_reset(&rx_sem);
	command_uart_test(0, NULL);
	if (k_sem_take(&rx_sem, K_MSEC(100))) {
		return EC_RES_BUSY;
	}

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_TP_SELF_TEST, cmd_touchpad_self_test, EC_VER_MASK(0));
