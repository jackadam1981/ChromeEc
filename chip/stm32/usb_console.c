/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "link_defs.h"
#include "printf.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define USB_CONSOLE_TIMEOUT_US (30 * MSEC)
#define USB_CONSOLE_RX_BUF_SIZE 16
#define RX_BUF_NEXT(i) (((i) + 1) & (USB_CONSOLE_RX_BUF_SIZE - 1))

volatile char rx_buf[USB_CONSOLE_RX_BUF_SIZE];
volatile int rx_buf_head;
volatile int rx_buf_tail;

/* USB-Serial descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_CONSOLE) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = USB_IFACE_CONSOLE,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_CONSOLE, 82) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | USB_EP_CON_TX,
	.bmAttributes = 0x02 /* Bulk IN */,
	.wMaxPacketSize = USB_MAX_PACKET_SIZE,
	.bInterval = 10
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_CONSOLE, 3) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_EP_CON_RX,
	.bmAttributes = 0x02 /* Bulk OUT */,
	.wMaxPacketSize = USB_MAX_PACKET_SIZE,
	.bInterval = 0
};

static usb_uint ep_buf_tx[USB_MAX_PACKET_SIZE / 2] __usb_ram;
static usb_uint ep_buf_rx[USB_MAX_PACKET_SIZE / 2] __usb_ram;

static void con_ep_tx(void)
{
	uint16_t ep = STM32_USB_EP(USB_EP_CON_TX);
	/* clear IT */
	STM32_USB_EP(USB_EP_CON_TX) = (ep & EP_MASK);
	return;
}

static void con_ep_rx(void)
{
	int i;
	for (i = 0; i < (btable_ep[USB_EP_CON_RX].rx_count & 0x3ff); i++) {
		int rx_buf_next = RX_BUF_NEXT(rx_buf_head);
		if (rx_buf_next != rx_buf_tail) {
			/* Not working on old STM32 ... */
			rx_buf[rx_buf_head] = ((uint8_t *)ep_buf_rx)[i];
			rx_buf_head = rx_buf_next;
		}
	}
	/* clear IT */
	STM32_TOGGLE_EP(USB_EP_CON_RX, EP_RX_MASK, EP_RX_VALID, 0);
	/* wake-up the console task */
	console_has_input();
	return;
}

int usb_getc(void)
{
	int c;

	if (rx_buf_tail == rx_buf_head)
		return -1;

	c = rx_buf[rx_buf_tail];
	rx_buf_tail = RX_BUF_NEXT(rx_buf_tail);
	return c;
}

static int tx_idx;
static int is_reset;

static int __tx_char(void *context, int c)
{
	uint16_t *buf = (uint16_t *)ep_buf_tx;

	/* Do newline to CRLF translation */
	if (c == '\n' && __tx_char(NULL, '\r'))
		return 1;

	if (tx_idx > 63)
		return 1;
	if (!(tx_idx & 1))
		buf[tx_idx/2] = c;
	else
		buf[tx_idx/2] |= c << 8;
	tx_idx++;

	return 0;
}

static inline int usb_console_tx_valid(void)
{
	return (STM32_USB_EP(USB_EP_CON_TX) & EP_TX_MASK) == EP_TX_VALID;
}

static int usb_wait_console(void)
{
	timestamp_t deadline = get_time();
	int wait_time_us = 1;
	deadline.val += USB_CONSOLE_TIMEOUT_US;

	while (usb_console_tx_valid()) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		if (wait_time_us < MSEC)
			udelay(wait_time_us);
		else
			usleep(wait_time_us);
		wait_time_us *= 2;
	}

	return EC_SUCCESS;
}

int usb_putc(int c)
{
	int ret;

	ret = usb_wait_console();
	if (ret)
		return ret;

	tx_idx = 0;
	ret = __tx_char(NULL, c);
	btable_ep[USB_EP_CON_TX].tx_count = tx_idx;
	/* enable TX */
	STM32_TOGGLE_EP(USB_EP_CON_TX, EP_TX_MASK, EP_TX_VALID, 0);

	return ret;
}

int usb_puts(const char *outstr)
{
	int ret;

	if (!is_reset)
		return 0;

	ret = usb_wait_console();
	if (ret)
		return ret;

	tx_idx = 0;
	/* Put all characters in the output buffer */
	while (*outstr) {
		if (__tx_char(NULL, *outstr++) != 0)
			break;
	}

	btable_ep[USB_EP_CON_TX].tx_count = tx_idx;
	/* enable TX */
	STM32_TOGGLE_EP(USB_EP_CON_TX, EP_TX_MASK, EP_TX_VALID, 0);
	/* Successful if we consumed all output */
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_vprintf(const char *format, va_list args)
{
	int ret;

	if (!is_reset)
		return 0;

	ret = usb_wait_console();
	if (ret)
		return ret;

	tx_idx = 0;
	ret = vfnprintf(__tx_char, NULL, format, args);

	btable_ep[USB_EP_CON_TX].tx_count = tx_idx;
	/* enable TX */
	STM32_TOGGLE_EP(USB_EP_CON_TX, EP_TX_MASK, EP_TX_VALID, 0);
	return ret;
}

static void ep_tx_reset(void)
{
	/* Serial Bulk IN endpoint 2 */
	btable_ep[USB_EP_CON_TX].tx_addr = usb_sram_addr(ep_buf_tx);
	btable_ep[USB_EP_CON_TX].tx_count = 0;
	btable_ep[USB_EP_CON_TX].rx_count = 0;
	STM32_USB_EP(USB_EP_CON_TX) = (USB_EP_CON_TX << 0) /* Endpoint Addr*/ |
				      (2 << 4) /* TX NAK */ |
				      (0 << 9) /* Bulk EP */ |
				      (0 << 12) /* RX Disabled */;
	is_reset = 1;
}

static void ep_rx_reset(void)
{
	/* Serial Bulk OUT endpoint 3 */
	btable_ep[USB_EP_CON_RX].rx_addr = usb_sram_addr(ep_buf_rx);
	btable_ep[USB_EP_CON_RX].tx_count = 0;
	btable_ep[USB_EP_CON_RX].rx_count =
		0x8000 | ((USB_MAX_PACKET_SIZE / 32 - 1) << 10);
	STM32_USB_EP(USB_EP_CON_RX) = (USB_EP_CON_RX << 0) /* Endpoint Addr */ |
				      (0 << 4) /* TX Disabled */ |
				      (0 << 9) /* Bulk EP */ |
				      (3 << 12) /* RX VALID */;
}

USB_DECLARE_EP(USB_EP_CON_TX, con_ep_tx, con_ep_tx, ep_tx_reset);
USB_DECLARE_EP(USB_EP_CON_RX, con_ep_rx, con_ep_rx, ep_rx_reset);
