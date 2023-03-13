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
#include "touchpad.h"
#include "usb_hid_touchpad.h"
#include "hid_i2c.h"

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

static void base_suspend_hook(struct ap_power_ev_callback *cb,
			      struct ap_power_ev_data data)
{
	switch (data.event) {
	case AP_POWER_SUSPEND:
		board_uart_tx(ROACH_CMD_SUSPEND, NULL, 0);
		break;
	case AP_POWER_RESUME:
		board_uart_tx(ROACH_CMD_RESUME, NULL, 0);
		break;
	default:
		return;
	}
}

static int ec_ec_comm_init(const struct device *unused)
{
	static struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, base_suspend_hook, AP_POWER_SUSPEND | AP_POWER_RESUME);
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

static void reassert_irq(void)
{
	if (k_msgq_num_used_get(&touchpad_report_queue) > 0) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_hid_int_odl), 0);
	}
}
DECLARE_DEFERRED(reassert_irq);

int board_target_cb(const uint8_t *in, int in_size, uint8_t *out)
{
	memset(out, 0, 0x100);

	if (in_size == 0) { /* read report? */
		int ret;

		ret = k_msgq_get(&touchpad_report_queue, out + 2, K_NO_WAIT);
		if (ret == 0) {
			*(uint16_t*)out = sizeof(struct usb_hid_touchpad_report);
		}

		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_hid_int_odl), 1);
		hook_call_deferred(&reassert_irq_data, MSEC);

		return ret ? 0 : sizeof(struct usb_hid_touchpad_report);
	}

	int reg = in[0];

	if (reg == 0x01) {
		memcpy(out, HID_DESC, sizeof(HID_DESC));
		return sizeof(HID_DESC);
	}

	if (reg == 0x02) {
		memcpy(out, REPORT_DESC, sizeof(REPORT_DESC));
		return sizeof(REPORT_DESC);
	}

	if (reg == 0x05) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_hid_int_odl), 0);
		return 0;
	}

	return 0;
}

static int cmd_debug(int argc, const char **argv)
{
	return 0;
}
DECLARE_CONSOLE_COMMAND(dbg, cmd_debug, "", "");
