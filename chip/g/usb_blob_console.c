/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "blob.h"
#include "console.h"
#include "printf.h"
#include "task.h"
#include "usb_descriptor.h"

#define BUF_SIZE 4000

static char tx_buf[BUF_SIZE];
static int tx_buf_tail;
static int tx_buf_head;
static uint8_t rx_buf[USB_MAX_PACKET_SIZE];
static int rx_buf_head;
static int rx_buf_tail;
static int is_enabled = 1;
static int is_readonly;

static int blob_has_output(void)
{
	if (tx_buf_tail < tx_buf_head)
		tx_buf_tail += blob_send_bytes(tx_buf + tx_buf_tail,
				       tx_buf_head - tx_buf_tail);
	else if (tx_buf_tail == tx_buf_head) {
		tx_buf_tail = 0;
		tx_buf_head = 0;
	}
	return 0;
}

static int __tx_char(void *context, int c)
{
	int *tx_idx = context;
	int tx_buf_next;

	tx_buf_next = *tx_idx + 1;
	if (tx_buf_next >= BUF_SIZE)
		return 1;

	tx_buf[*tx_idx] = c;
	*tx_idx = tx_buf_next;

	return EC_SUCCESS;
}

/*
 * Public USB console implementation below.
 */
int usb_getc(void)
{
	int c;

	if (rx_buf_tail >= rx_buf_head)
		return -1;

	if (!is_enabled)
		return -1;

	c = rx_buf[rx_buf_tail];
	rx_buf_tail++;
	return c;
}

int usb_putc(int c)
{
	int ret;

	if (is_readonly)
		return EC_SUCCESS;

	ret = __tx_char(&tx_buf_head, c);
	blob_has_output();

	return ret;
}

int usb_puts(const char *outstr)
{
	int ret = 0;

	if (is_readonly)
		return EC_SUCCESS;

	/* Put all characters in the output buffer */
	while (*outstr) {
		ret = __tx_char(&tx_buf_head, *outstr++);
		if (ret != 0)
			break;
	}
	blob_has_output();

	/* Successful if we consumed all output */
	return ret ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_vprintf(const char *format, va_list args)
{
	int ret;

	if (is_readonly)
		return EC_SUCCESS;

	ret = vfnprintf(__tx_char, &tx_buf_head, format, args);
	blob_has_output();
	return ret;
}

void usb_console_enable(int enabled, int readonly)
{
	is_enabled = enabled;
	is_readonly = readonly;
}

void blob_sent_output(void)
{
	blob_has_output();
}


void blob_has_input(void)
{
	rx_buf_head = blob_get_bytes(rx_buf, sizeof(rx_buf));
	rx_buf_tail = 0;
	if (rx_buf_head)
		task_wake(TASK_ID_CONSOLE);
}
