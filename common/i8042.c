/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Chrome OS EC i8042 interface code.
 */

#include "board.h"
#include "common.h"
#include "console.h"
#include "i8042.h"
#include "keyboard.h"
#include "task.h"
#include "timer.h"
#include "util.h"


#define I8042_DEBUG 1

/* Console output macros */
#if I8042_DEBUG >= 4
#define CPRINTF4(format, args...) cprintf(CC_I8042, format, ## args)
#else
#define CPRINTF4(format, args...)
#endif
#if I8042_DEBUG >= 5
#define CPRINTF5(format, args...) cprintf(CC_I8042, format, ## args)
#else
#define CPRINTF5(format, args...)
#endif

#define MAX_QUEUED_KEY_PRESS 16

static int i8042_irq_enabled;


/* Circular buffer
 *   head: next to dequeqe
 *   tail: next to enqueue
 *   head == tail: empty.
 *   tail + 1 == head: full
 */
struct circular_buffer {
	struct mutex mutex;
	int head, tail;
	int size;      /* size of buffer (in byte) */
	int unit;      /* size of unit (in byte) */
	uint8_t *buf;
};

#define TO_HOST_BUFSIZE (16)
static uint8_t to_host_buffer[TO_HOST_BUFSIZE];
static struct circular_buffer to_host = {
	.size = ARRAY_SIZE(to_host_buffer),
	.unit = sizeof(uint8_t),
	.buf  = to_host_buffer,
};

/* Queue command/data from the host */
enum {
	HOST_COMMAND = 0,
	HOST_DATA,
};
struct host_byte {
	uint8_t type;
	uint8_t byte;
};
/* 4 is big enough for all i8042 commands */
#define FROM_HOST_BUFSIZE (4 * sizeof(struct host_byte))
static uint8_t from_host_buffer[FROM_HOST_BUFSIZE];
static struct circular_buffer from_host = {
	/* The mutex is not needed since
	 *  1. no race condition between the LPC interrupt and task.
	 *  2. only one thread dequeues it.
	 */
	.size = ARRAY_SIZE(from_host_buffer),
	.unit = sizeof(struct host_byte),
	.buf  = from_host_buffer,
};


/* circular helpers */
static void circular_reset(struct circular_buffer *queue)
{
	queue->head = queue->tail = 0;
}

static int circular_empty(struct circular_buffer *queue)
{
	return queue->head == queue->tail;
}

static int circular_has_space(struct circular_buffer *queue, int count)
{
	return (queue->tail + count * queue->unit) <=
	       (queue->head + queue->size - queue->unit);
}

static void circular_enqueue_one(struct circular_buffer *queue,
				 const void *bytes, const int len)
{
	int i;

	/* the length must be the multiple of unit */
	ASSERT((len % queue->unit) == 0);

	if (!circular_has_space(queue, 1))
		return;

	for (i = 0; i < len; ++i) {
		queue->buf[queue->tail++] = ((char *)bytes)[i];
		queue->tail %= queue->size;
	}
}

static int circular_dequeue_one(struct circular_buffer *queue, void *bytes)
{
	int i;

	if (circular_empty(queue))
		return 0;

	for (i = 0; i < queue->unit; i++) {
		((char *)bytes)[i] = queue->buf[queue->head];
		queue->head = (queue->head + 1) % queue->size;
	}

	return 1;
}


/* Reset all i8042 buffer */
void i8042_flush_buffer()
{
	circular_reset(&to_host);
	keyboard_clear_buffer();
}


/* Called by the chip-specific code when host sedns a byte to port 0x60.
 * Note that this is in the interrupt context.
 */
void i8042_receives_data(int data)
{
	struct host_byte h;

	h.type = HOST_DATA;
	h.byte = data;
	circular_enqueue_one(&from_host, &h, sizeof(h));
	task_wake(TASK_ID_I8042CMD);
}


/* Called by the chip-specific code when host sedns a byte to port 0x64.
 * Note that this is in the interrupt context.
 */
void i8042_receives_command(int cmd)
{
	struct host_byte h;

	h.type = HOST_COMMAND;
	h.byte = cmd;
	circular_enqueue_one(&from_host, &h, sizeof(h));
	task_wake(TASK_ID_I8042CMD);
}


/* Called by common/keyboard.c when the host wants to receive keyboard IRQ
 * (or not).
 */
void i8042_enable_keyboard_irq(void) {
	i8042_irq_enabled = 1;
	keyboard_resume_interrupt();
}

void i8042_disable_keyboard_irq(void) {
	i8042_irq_enabled = 0;
}


static void i8042_handle_from_host(void)
{
	struct host_byte h;
	int ret_len;
	uint8_t output[MAX_SCAN_CODE_LEN];
	enum ec_error_list ret;

	while (circular_dequeue_one(&from_host, &h)) {
		if (h.type == HOST_COMMAND)
			ret_len = handle_keyboard_command(h.byte, output);
		else
			ret_len = handle_keyboard_data(h.byte, output);

		ret = i8042_send_to_host(ret_len, output);
		ASSERT(ret == EC_SUCCESS);
	}
}

void i8042_command_task(void)
{
	while (1) {
		/* Either a new byte to host or host picking up can un-block. */
		task_wait_event(-1);

		while (1) {
			uint8_t chr;

			/* first handle command/data from host. */
			i8042_handle_from_host();

			/* Check if we have data in buffer to host. */
			if (circular_empty(&to_host))
				break;  /* nothing to host */

			/* if the host still didn't read that away,
			   try next time. */
			if (keyboard_has_char()) {
				CPRINTF5("[%T i8042_command_task() "
					 "cannot send to host due to host "
					 "haven't taken away.\n");
				break;
			}

			/* Get a char from buffer. */
			kblog_put('k', to_host.head);
			circular_dequeue_one(&to_host, &chr);
			kblog_put('K', chr);

			/* Write to host. */
			keyboard_put_char(chr, i8042_irq_enabled);
			CPRINTF4("[%T i8042_command_task() "
				 "sends to host: 0x%02x\n", chr);
		}
	}
}


static void enq_to_host(int len, const uint8_t *bytes)
{
	int i;

	mutex_lock(&to_host.mutex);
	/* Check if the buffer has enough space, then copy them to buffer. */
	if (circular_has_space(&to_host, len)) {
		for (i = 0; i < len; ++i) {
			kblog_put('t', to_host.tail);
			kblog_put('T', bytes[i]);
		}
		circular_enqueue_one(&to_host, bytes, len);
	}
	mutex_unlock(&to_host.mutex);
}

enum ec_error_list i8042_send_to_host(int len, const uint8_t *bytes)
{
	int i;

	for (i = 0; i < len; i++)
		kblog_put('s', bytes[i]);

	/* Put to queue in memory */
	enq_to_host(len, bytes);

	/* Wake up the task to move from queue to the buffer to host. */
	task_wake(TASK_ID_I8042CMD);

	return EC_SUCCESS;
}
