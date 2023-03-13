/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "ap_power/ap_power.h"
#include "console.h"
#include "hooks.h"
#include "keyboard_mkbp.h"
#include "mkbp_event.h"
#include "mkbp_fifo.h"
#include "one_wire.h"
#include "usb_hid_touchpad.h"

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart2));

K_MSGQ_DEFINE(touchpad_report_queue, sizeof(struct usb_hid_touchpad_report), 16, 4);

void board_process_packet(const struct RoachMessage *msg)
{
	if (msg->header.cmd == ROACH_CMD_KEYBOARD_MATRIX) {
		mkbp_keyboard_add(msg->payload);
	}
	if (msg->header.cmd == ROACH_CMD_TOUCHPAD_REPORT) {
		k_msgq_put(&touchpad_report_queue, msg->payload, K_NO_WAIT);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_hid_int_odl), 0);
	}
}

static void base_shutdown_hook(struct ap_power_ev_callback *cb,
			      struct ap_power_ev_data data)
{
	switch (data.event) {
	case AP_POWER_SHUTDOWN:
		board_uart_tx(ROACH_CMD_SUSPEND, NULL, 0);
		break;
	case AP_POWER_STARTUP:
		board_uart_tx(ROACH_CMD_RESUME, NULL, 0);
		break;
	default:
		return;
	}
}

static int ec_ec_comm_init(void)
{
	static struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, base_shutdown_hook, AP_POWER_STARTUP | AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);

	mode = MODE_LID;
	uart_irq_callback_user_data_set(uart, uart_handler, NULL);
	uart_irq_rx_enable(uart);

	i2c_target_driver_register(DEVICE_DT_GET(DT_NODELABEL(i2c5_target)));

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

