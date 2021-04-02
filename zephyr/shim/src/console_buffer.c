/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/uart.h>
#include <kernel.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "uart.h"

static char console_buf[CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE_BUF_SIZE];
static uint32_t previous_snapshot_idx;
static uint32_t current_snapshot_idx;
static uint32_t tail_idx;

static struct k_poll_signal shell_uninit_signal;
static struct k_poll_signal shell_init_signal;
RING_BUF_DECLARE(rx_buffer, CONFIG_UART_RX_BUF_SIZE);

static void fork_uart_rx_handle(const struct device *dev)
{
	static uint8_t scratch;
	static uint8_t *data;
	static uint32_t len, rd_len;

	do {
		len = ring_buf_put_claim(&rx_buffer, &data, rx_buffer.size);
		if (len > 0) {
			rd_len = uart_fifo_read(dev, data, len);
			ring_buf_put_finish(&rx_buffer, rd_len);
		} else {
			rd_len = uart_fifo_read(dev, &scratch, 1);
		}
	} while (rd_len != 0 && rd_len == len);
}

static void fork_uart_callback(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);

	if (uart_irq_rx_ready(dev))
		fork_uart_rx_handle(dev);
}

static void shell_uninit_callback(const struct shell *shell, int res)
{
	const struct device *dev =
		device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);

	if (!res) {
		/* Set the new callback. */
		uart_irq_callback_user_data_set(dev, fork_uart_callback, NULL);
		uart_irq_tx_disable(dev);
		uart_irq_rx_enable(dev);
	}

	k_poll_signal_raise(&shell_uninit_signal, res);
}

int uart_shell_stop(void)
{
	struct k_poll_event events[1] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
					 K_POLL_MODE_NOTIFY_ONLY,
					 &shell_uninit_signal),
	};
	const struct device *dev =
		device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);

	uart_clear_input();
	uart_irq_rx_disable(dev);
	uart_irq_tx_disable(dev);
	k_poll_signal_init(&shell_uninit_signal);
	shell_uninit(shell_backend_uart_get_ptr(), shell_uninit_callback);

	/* Wait for the shell to be turned off, the signal will wake us. */
	k_poll(events, 1, K_FOREVER);

	/* Event was signaled. */
	return events[0].signal->result;
}

static void shell_init_from_work(struct k_work *work)
{
	const struct device *dev =
		device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	uint32_t level =
		(CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
			CONFIG_LOG_MAX_LEVEL :
			CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;
	ARG_UNUSED(work);

	shell_init(shell_backend_uart_get_ptr(), dev, false, log_backend,
		   level);
	uart_irq_rx_enable(dev);
	uart_irq_tx_enable(dev);

	k_poll_signal_raise(&shell_init_signal, 0);
}

void uart_shell_start(void)
{
	static struct k_work shell_init_work;
	const struct device *dev =
		device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);
	struct k_poll_event events[1] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
					 K_POLL_MODE_NOTIFY_ONLY,
					 &shell_init_signal),
	};

	uart_irq_rx_disable(dev);
	uart_irq_tx_disable(dev);
	k_work_init(&shell_init_work, shell_init_from_work);
	k_poll_signal_init(&shell_init_signal);
	k_work_submit(&shell_init_work);

	/* Wait for initialization to be run. */
	k_poll(events, 1, K_FOREVER);
}

static inline uint32_t next_idx(uint32_t cur_idx)
{
	return (cur_idx + 1) % ARRAY_SIZE(console_buf);
}

K_MUTEX_DEFINE(console_write_lock);

void console_buf_notify_char(char c)
{
	/* Don't copy null byte into buffer */
	if (!c)
		return;

	/*
	 * This is just notifying of a console character for debugging
	 * output, so if we are unable to lock the mutex immediately,
	 * then just drop the character.
	 */
	if (!k_mutex_lock(&console_write_lock, K_NO_WAIT)) {
		/* We got the mutex. */
		uint32_t new_tail = next_idx(tail_idx);

		/* Check if we are starting to overwrite our snapshot heads */
		if (new_tail == previous_snapshot_idx)
			previous_snapshot_idx = next_idx(previous_snapshot_idx);
		if (new_tail == current_snapshot_idx)
			current_snapshot_idx = next_idx(current_snapshot_idx);

		console_buf[new_tail] = c;
		tail_idx = new_tail;
		k_mutex_unlock(&console_write_lock);
	}
}

enum ec_status uart_console_read_buffer_init(void)
{
	if (k_mutex_lock(&console_write_lock, K_MSEC(100)))
		/* Failed to acquire console buffer mutex */
		return EC_RES_TIMEOUT;

	previous_snapshot_idx = current_snapshot_idx;
	current_snapshot_idx = tail_idx;

	k_mutex_unlock(&console_write_lock);

	return EC_RES_SUCCESS;
}

int uart_console_read_buffer(uint8_t type, char *dest, uint16_t dest_size,
			     uint16_t *write_count_out)
{
	uint32_t head;
	uint16_t write_count = 0;

	switch (type) {
	case CONSOLE_READ_NEXT:
		/* Start from beginning of latest snapshot */
		head = current_snapshot_idx;
		break;
	case CONSOLE_READ_RECENT:
		/* Start from end of previous snapshot */
		head = previous_snapshot_idx;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	if (head == tail_idx)
		/* No new data, return empty response */
		return EC_RES_SUCCESS;

	/* We need to make sure we have room for at least the null byte */
	if (dest_size == 0)
		return EC_RES_INVALID_PARAM;

	do {
		if (write_count >= dest_size - 1)
			/* Buffer is full, minus the space for a null byte */
			break;

		dest[write_count] = console_buf[head];
		write_count++;
		head = next_idx(head);
	} while (head != tail_idx);

	dest[write_count] = '\0';
	write_count++;

	*write_count_out = write_count;

	return EC_RES_SUCCESS;
}

/* ECOS uart buffer, putc is blocking instead. */
int uart_buffer_full(void)
{
	return false;
}

int uart_getc(void)
{
	uint8_t c;

	if (ring_buf_get(&rx_buffer, &c, 1)) {
		return c;
	}
	return -1;
}

void uart_clear_input(void)
{
	/* Clear any remaining shell processing. */
	shell_process(shell_backend_uart_get_ptr());
}
