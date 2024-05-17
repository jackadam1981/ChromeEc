/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "ec_commands.h"

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/time_units.h>

static char console_buf[CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE_BUF_SIZE];
static uint32_t previous_snapshot_idx;
static uint32_t current_snapshot_idx;
static uint32_t read_next_idx;
static uint32_t head_idx;
static uint32_t tail_idx;

static inline uint32_t next_idx(uint32_t cur_idx)
{
	return (cur_idx + 1) % ARRAY_SIZE(console_buf);
}

K_MUTEX_DEFINE(console_write_lock);

static struct {
	int64_t notify_chars_count;
	int64_t null;
	int64_t written;
	int64_t discarded;
	int64_t overflow;
	int64_t read;
} traffic;

static struct {
	const int64_t when;
	int64_t completed_ts;
} snapshots[] = {
	{ .when = 30000, .completed_ts = 0},
	{ .when = 60000, .completed_ts = 0},
	{ .when = 90000, .completed_ts = 0},
	{ .when = 120000, .completed_ts = 0},
	{ .when = 150000, .completed_ts = 0},
	{ .when = 180000, .completed_ts = 0},
};
static int snapshot_index = 0;

static void print_traffic() {
	int64_t now = k_uptime_get();

	if (snapshot_index >= ARRAY_SIZE(snapshots) ||
	    snapshots[snapshot_index].when < now) {
		return;
	}

	int64_t duration = snapshot_index == 0
				   ? (now)
				   : (now - snapshots[snapshot_index - 1].completed_ts);
	int64_t urate_null = (traffic.null * 1000L * 1000000L) / duration;
	int64_t urate_written = (traffic.written * 1000L * 1000000L) / duration;
	int64_t urate_discarded = (traffic.discarded * 1000L * 1000000L) / duration;
	int64_t urate_overflow = (traffic.overflow * 1000L * 1000000L) / duration;
	int64_t urate_read = (traffic.read * 1000L * 1000000L) / duration;

	printk("\n%" PRIi64 ",%" PRIi64 ",%" PRIi64 ".%06" PRIi64 ","
	       "%" PRIi64 ".%06" PRIi64 ",%" PRIi64 ".%06" PRIi64 ","
	       "%" PRIi64 ".%06" PRIi64 ",%" PRIi64 ".%06" PRIi64 "\n",
	       now, traffic.notify_chars_count,
	       urate_written / 1000000L, urate_written % 1000000L,
	       urate_null / 1000000L, urate_null % 1000000L,
	       urate_discarded / 1000000L, urate_discarded % 1000000L,
	       urate_overflow / 1000000L, urate_overflow % 1000000L,
	       urate_read / 1000000L, urate_read % 1000000L);

	snapshots[snapshot_index++].completed_ts = now;
	traffic.notify_chars_count = 0;
	traffic.null = 0;
	traffic.written = 0;
	traffic.discarded = 0;
	traffic.overflow = 0;
	traffic.read = 0;
}

size_t console_buf_notify_chars(const char *s, size_t len)
{
	/*
	 * This is just notifying of console characters for debugging
	 * output, so if we are unable to lock the mutex immediately,
	 * then just drop the string.
	 */
	traffic.notify_chars_count++;
	if (k_mutex_lock(&console_write_lock, K_NO_WAIT)) {
		traffic.discarded += len;
		return 0;
	}
	/* We got the mutex. */
	for (size_t i = 0; i < len; i++) {
		/* Don't copy null byte into buffer */
		if (!(*s)) {
			s++;
			traffic.null++;
			continue;
		}

		uint32_t new_tail = next_idx(tail_idx);

		/* Check if we are starting to overwrite our snapshot
		 * heads
		 */
		if (new_tail == head_idx) {
			head_idx = next_idx(head_idx);
		}
		if (new_tail == previous_snapshot_idx) {
			previous_snapshot_idx = next_idx(previous_snapshot_idx);
		}
		if (new_tail == current_snapshot_idx) {
			current_snapshot_idx = next_idx(current_snapshot_idx);
		}
		if (new_tail == read_next_idx) {
			traffic.overflow++;
			read_next_idx = next_idx(read_next_idx);
		}

		console_buf[tail_idx] = *s++;
		tail_idx = new_tail;
		traffic.written++;
	}
	k_mutex_unlock(&console_write_lock);
	return len;
}

enum ec_status uart_console_read_buffer_init(void)
{
	if (k_mutex_lock(&console_write_lock, K_MSEC(100)))
		/* Failed to acquire console buffer mutex */
		return EC_RES_TIMEOUT;

	/* For read next, start reading at the beginning of the buffer */
	read_next_idx = head_idx;
	/*
	 * For read recent, start reading at the beginning of the previous
	 * snapshot
	 */
	previous_snapshot_idx = current_snapshot_idx;
	/*
	 * Limit read command to characters available at the moment of creating
	 * snapshot
	 */
	current_snapshot_idx = tail_idx;

	k_mutex_unlock(&console_write_lock);

	return EC_RES_SUCCESS;
}

int uart_console_read_buffer(uint8_t type, char *dest, uint16_t dest_size,
			     uint16_t *write_count_out)
{
	uint32_t *head;
	uint16_t write_count = 0;

	switch (type) {
	case CONSOLE_READ_NEXT:
		/*
		 * Start where we left or from the beginning of the buffer after
		 * snapshot
		 */
		head = &read_next_idx;
		break;
	case CONSOLE_READ_RECENT:
		/* Start from end of previous snapshot */
		head = &previous_snapshot_idx;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	/* We need to make sure we have room for at least the null byte */
	if (dest_size == 0)
		return EC_RES_INVALID_PARAM;

	if (k_mutex_lock(&console_write_lock, K_MSEC(100)))
		/* Failed to acquire console buffer mutex */
		return EC_RES_TIMEOUT;

	if (*head == current_snapshot_idx) {
		/* No new data, return empty response */
		k_mutex_unlock(&console_write_lock);
		*write_count_out = 0;
		return EC_RES_SUCCESS;
	}

	do {
		if (write_count >= dest_size - 1)
			/* Buffer is full, minus the space for a null byte */
			break;

		dest[write_count] = console_buf[*head];
		write_count++;
		*head = next_idx(*head);
	} while (*head != current_snapshot_idx);

	dest[write_count] = '\0';
	write_count++;

	traffic.read += write_count;
	print_traffic();

	*write_count_out = write_count;
	k_mutex_unlock(&console_write_lock);

	return EC_RES_SUCCESS;
}

/* ECOS uart buffer, putc is blocking instead. */
int uart_buffer_full(void)
{
	return false;
}
