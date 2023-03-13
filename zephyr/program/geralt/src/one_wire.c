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
#include "host_command.h"
#include "keyboard_mkbp.h"
#include "mkbp_event.h"
#include "mkbp_fifo.h"
#include "one_wire.h"
#include "queue.h"
#include "touchpad.h"
#include "usb_hid_touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

void board_process_packet(const struct RoachMessage *msg)
{
	if (msg->header.cmd == ROACH_CMD_KEYBOARD_MATRIX) {
		mkbp_keyboard_add(msg->payload);
	}
	if (msg->header.cmd == ROACH_CMD_TOUCHPAD_REPORT) {
		mkbp_fifo_add(EC_MKBP_EVENT_TOUCHPAD, msg->payload);
	}
}

static int ec_ec_comm_init(const struct device *unused)
{
	mode = MODE_LID;
	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	/* UART1PMR */
	*(volatile uint8_t*)0xf03a23 = 1;

	uint8_t lcr_cache = *(volatile uint8_t*)0xf02803;
	*(volatile uint8_t*)0xf02803 |= 0x80; /* access divisor latches */
	*(volatile uint8_t*)0xf02800 = 0x01; /* set divisor = 0x8001 */
	*(volatile uint8_t*)0xf02801 = 0x80;
	*(volatile uint8_t*)0xf02808 = 2; /* high speed select */
	*(volatile uint8_t*)0xf02803 = lcr_cache;

	return 0;
}
SYS_INIT(ec_ec_comm_init, APPLICATION, 1);

static int touchpad_get_next_event(uint8_t *out)
{
	return mkbp_fifo_get_next_event(out, EC_MKBP_EVENT_TOUCHPAD);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_TOUCHPAD, touchpad_get_next_event);
