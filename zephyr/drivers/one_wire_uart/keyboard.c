/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "consumer.h"
#include "drivers/one_wire_uart.h"
#include "drivers/one_wire_uart_internal.h"
#include "drivers/one_wire_uart_stream.h"
#include "hooks.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "queue.h"
#include "touchpad.h"
#include "usb_hid_touchpad.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#define CPRINTS(format, args...) cprints(CC_USB, format, ##args)

const static struct device *one_wire_uart =
	DEVICE_DT_GET(DT_NODELABEL(one_wire_uart));

void updater_stream_written(const struct consumer *consumer, size_t count)
{
	while (!queue_is_empty(consumer->queue)) {
		struct queue_chunk chunk =
			queue_get_read_chunk(consumer->queue);
		int ret;

		ret = one_wire_uart_send(one_wire_uart,
					 ROACH_CMD_UPDATER_COMMAND,
					 chunk.buffer, chunk.count);
		if (ret) {
			CPRINTS("%s: tx queue full", __func__);
		}
		queue_advance_head(consumer->queue, chunk.count);
	}
}

#define TP_NODE DT_INST(0, elan_ekth3000)
#define CONFIG_TOUCHPAD_I2C_ADDR_FLAGS DT_REG_ADDR(TP_NODE)
#define CONFIG_TOUCHPAD_I2C_PORT I2C_PORT_BY_DEV(TP_NODE)

static void recv_cb(uint8_t cmd, const uint8_t *payload, int length)
{
	/* TODO(b/277667319): handle ROACH_CMD_SUSPEND/RESUME after touchpad
	 * driver ready.
	 */

	if (cmd == ROACH_CMD_UPDATER_COMMAND) {
		const struct queue *usb_to_update = usb_update.producer.queue;

		QUEUE_ADD_UNITS(usb_to_update, payload, length);
	}

	if (cmd == ROACH_CMD_TP_PASSTHRU) {
		static uint8_t writebuf[2048] = {};
		static int writebuf_count = 0;

		bool is_last_chunk;
		static uint8_t readbuf[2048] = {};
		int readcount;
		int rv;

		if (system_is_locked()) {
			return;
		}

		if (length < 1) {
			return;
		}

		is_last_chunk = !!payload[0];

		memcpy(writebuf + writebuf_count, payload + 1, length - 1);
		writebuf_count += length - 1;

		if (!is_last_chunk) {
			return;
		}

		readcount = sys_get_le16(writebuf);
		readcount = MIN(readcount, sizeof(readbuf));

		rv = i2c_xfer(CONFIG_TOUCHPAD_I2C_PORT,
			      CONFIG_TOUCHPAD_I2C_ADDR_FLAGS, writebuf + 2,
			      writebuf_count - 2, readbuf, readcount);

		if (!rv) {
			uint8_t *ptr = readbuf;

			while (readcount > 0) {
				int transfer_size = MIN(readcount, 32);

				one_wire_uart_send(one_wire_uart,
						   ROACH_CMD_TP_PASSTHRU, ptr,
						   transfer_size);
				readcount -= transfer_size;
				ptr += transfer_size;
			}
		}
		writebuf_count = 0;
	}
}

static void ec_ec_comm_init(void)
{
	one_wire_uart_set_callback(one_wire_uart, recv_cb);
	one_wire_uart_enable(one_wire_uart);
}
DECLARE_HOOK(HOOK_INIT, ec_ec_comm_init, HOOK_PRIO_DEFAULT);

void keyboard_state_changed(int row, int col, int is_pressed)
{
	uint8_t state[KEYBOARD_COLS_MAX];

	memcpy(state, keyboard_scan_get_state(), KEYBOARD_COLS_MAX);
	if (is_pressed) {
		state[col] |= BIT(row);
	} else {
		state[col] &= ~BIT(row);
	}

	one_wire_uart_send(one_wire_uart, ROACH_CMD_KEYBOARD_MATRIX, state,
			   KEYBOARD_COLS_MAX);
}

void set_touchpad_report(struct usb_hid_touchpad_report *report)
{
	one_wire_uart_send(one_wire_uart, ROACH_CMD_TOUCHPAD_REPORT,
			   (uint8_t *)report, sizeof(*report));
}
