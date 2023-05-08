/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "console.h"
#include "one_wire.h"
#include "keyboard_scan.h"
#include "touchpad.h"
#include "usb_hid_touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

/* TODO: move to header */
int elan_tp_set_power(int);

void board_process_packet(const struct RoachMessage *msg)
{
	if (msg->header.cmd == ROACH_CMD_SUSPEND) {
		elan_tp_set_power(0);
	}
	if (msg->header.cmd == ROACH_CMD_RESUME) {
		elan_tp_set_power(1);
	}
}

static int ec_ec_comm_init(void)
{
	mode = MODE_BASE;

	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	uint8_t lcr_cache = *(volatile uint8_t*)0xf02803;
	*(volatile uint8_t*)0xf02803 |= 0x80; /* access divisor latches */
	*(volatile uint8_t*)0xf02800 = 0x01; /* set divisor = 0x8001 */
	*(volatile uint8_t*)0xf02801 = 0x80;
	*(volatile uint8_t*)0xf02808 = 2; /* high speed select */
	*(volatile uint8_t*)0xf02803 = lcr_cache;

	return 0;
}
SYS_INIT(ec_ec_comm_init, APPLICATION, 50);

void detachable_keyboard_add(const uint8_t *state)
{
	/* board_uart_tx(ROACH_CMD_KEYBOARD_MATRIX, state, KEYBOARD_COLS_MAX); */
}

void board_touchpad_reset(void)
{
}

void set_touchpad_report(struct usb_hid_touchpad_report *report)
{
	/* board_uart_tx(ROACH_CMD_TOUCHPAD_REPORT, (uint8_t*)report, sizeof(*report)); */
}
