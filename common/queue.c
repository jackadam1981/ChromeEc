/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Queue data structure implementation.
 */
#include "queue.h"
#include "util.h"

void queue_reset(struct queue *queue)
{
	queue->head = queue->tail = 0;
}

int queue_is_empty(const struct queue *q)
{
	return q->head == q->tail;
}

int queue_has_space(const struct queue *q, int unit_count)
{
	return (q->tail + unit_count * q->unit_sizes) <=
	       (q->head + q->buf_sizes - q->unit_sizes);
}

void queue_add_units(struct queue *q, const void *src, int unit_count)
{
	const uint8_t *s = (const uint8_t *)src;

	if (!queue_has_space(q, unit_count))
		return;

	for (unit_count *= q->unit_sizes; unit_count; unit_count--) {
		q->buf[q->tail++] = *(s++);
		q->tail %= q->buf_sizes;
	}
}

int queue_remove_unit(struct queue *q, void *dest)
{
	int count;
	uint8_t *d = (uint8_t *)dest;

	if (queue_is_empty(q))
		return 0;

	for (count = q->unit_sizes; count; count--) {
		*(d++) = q->buf[q->head++];
		q->head %= q->buf_sizes;
	}

	return 1;
}
