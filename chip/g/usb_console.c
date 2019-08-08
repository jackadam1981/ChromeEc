/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "crc.h"
#include "link_defs.h"
#include "printf.h"
#include "queue.h"
#include "task.h"
#include "timer.h"
#include "usb-stream.h"

/* Console output macro */
#define USB_CONSOLE_TIMEOUT_US (30 * MSEC)

#define QUEUE_SIZE_USB_TX     4096
#define QUEUE_SIZE_USB_RX     USB_MAX_PACKET_SIZE

static void usb_console_wr(struct queue_policy const *policy, size_t count);
static void uart_console_rd(struct queue_policy const *policy, size_t count);


static int last_tx_ok = 1;

/*
 * Start enabled, so we can queue early debug output before the board gets
 * around to calling usb_console_enable().
 */
static int is_enabled = 1;

/*
 * But start read-only, so we don't accept console input until we explicitly
 * decide that we're ready for it.
 */
static int is_readonly = 1;

/*
 * This is a usb_console producer policy, which wakes up CONSOLE task whenever
 * rx_q gets new data added. This shall be called by rx_stream_handler() in
 * usb-stream.c.
 */
static struct queue_policy const usb_console_policy = {
	.add    = usb_console_wr,
	.remove = uart_console_rd,
};

static struct queue const tx_q = QUEUE_NULL(QUEUE_SIZE_USB_TX, uint8_t);
static struct queue const rx_q = QUEUE(QUEUE_SIZE_USB_RX, uint8_t,
				       usb_console_policy);

struct usb_stream_config const usb_console;

USB_STREAM_CONFIG(usb_console,
		  USB_IFACE_CONSOLE,
		  USB_STR_CONSOLE_NAME,
		  USB_EP_CONSOLE,
		  USB_MAX_PACKET_SIZE,
		  USB_MAX_PACKET_SIZE,
		  rx_q,
		  tx_q)

static void usb_console_wr(struct queue_policy const *policy, size_t count)
{
	task_wake(TASK_ID_CONSOLE);
}

static void uart_console_rd(struct queue_policy const *policy, size_t count)
{
	/* do nothing */
}

static void handle_output(void)
{
	/* Wake up the Tx FIFO handler */
	usb_console.consumer.ops->written(&usb_console.consumer, 1);
}

static int usb_wait_console(void)
{
	timestamp_t deadline = get_time();
	int wait_time_us = 1;

	if (!is_enabled || !tx_fifo_is_ready(&usb_console))
		return EC_SUCCESS;

	deadline.val += USB_CONSOLE_TIMEOUT_US;

	/*
	 * If the USB console is not used, Tx buffer would never free up.
	 * In this case, let's drop characters immediately instead of sitting
	 * for some time just to time out. On the other hand, if the last
	 * Tx is good, it's likely the host is there to receive data, and
	 * we should wait so that we don't clobber the buffer.
	 */
	if (last_tx_ok) {
		while (queue_space(&tx_q) < USB_MAX_PACKET_SIZE ||
		       !*usb_console.is_reset) {
			if (timestamp_expired(deadline, NULL) ||
			    in_interrupt_context()) {
				last_tx_ok = 0;
				return EC_ERROR_TIMEOUT;
			}
			if (wait_time_us < MSEC)
				udelay(wait_time_us);
			else
				usleep(wait_time_us);
			wait_time_us *= 2;
		}

		return EC_SUCCESS;
	} else {
		last_tx_ok = queue_space(&tx_q);
		return EC_SUCCESS;
	}
}

#ifdef CONFIG_USB_CONSOLE_CRC
static uint32_t usb_tx_crc_ctx;

void usb_console_crc_init(void)
{
	crc32_ctx_init(&usb_tx_crc_ctx);
}

uint32_t usb_console_crc(void)
{
	return crc32_ctx_result(&usb_tx_crc_ctx);
}
#endif

static int __tx_char(void *context, int c)
{
	struct queue *state =
			(struct queue *) context;

	if (c == '\n' && __tx_char(state, '\r'))
		return 1;

#ifdef CONFIG_USB_CONSOLE_CRC
	crc32_ctx_hash8(&usb_tx_crc_ctx, c);

	while (QUEUE_ADD_UNITS(state, &c, 1) != 1)
		usleep(500);
#else
	QUEUE_ADD_UNITS(state, &c, 1);
#endif
	return 0;
}

/*
 * Public USB console implementation below.
 */
int usb_getc(void)
{
	int c;

	if (is_readonly || !is_enabled)
		return -1;

	if (QUEUE_REMOVE_UNITS(&rx_q, &c, 1))
		return c;
	return -1;
}

int usb_puts(const char *outstr)
{
	int ret;
	struct queue state;

	if (!is_enabled)
		return EC_SUCCESS;

	ret  = usb_wait_console();
	if (ret)
		return ret;

	state = tx_q;
	while (*outstr)
		if (__tx_char(&state, *outstr++))
			break;

	if (queue_count(&state))
		handle_output();

	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_putc(int c)
{
	char string[2];

	string[0] = c;
	string[1] = '\0';
	return usb_puts(string);
}

int usb_vprintf(const char *format, va_list args)
{
	int ret;
	struct queue state;

	if (!is_enabled)
		return EC_SUCCESS;

	ret = usb_wait_console();
	if (ret)
		return ret;

	state = tx_q;
	ret = vfnprintf(__tx_char, &state, format, args);

	if (queue_count(&state))
		handle_output();

	return ret;
}

void usb_console_enable(int enabled, int readonly)
{
	is_enabled = enabled;
	is_readonly = readonly;
}
