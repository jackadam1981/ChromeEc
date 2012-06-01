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

int queue_empty(struct queue *queue)
{
	return queue->head == queue->tail;
}

int queue_has_space(struct queue *queue, const int count)
{
	return (queue->tail + count * queue->unit) <=
	       (queue->head + queue->size - queue->unit);
}

void queue_enqueue(struct queue *queue,
		   const void *bytes, const int count)
{
	int i;

	if (!queue_has_space(queue, count))
		return;

	for (i = 0; i < queue->unit * count; ++i) {
		queue->buf[queue->tail++] = ((char *)bytes)[i];
		queue->tail %= queue->size;
	}
}

int queue_dequeue_one(struct queue *queue, void *bytes)
{
	int i;

	if (queue_empty(queue))
		return 0;

	for (i = 0; i < queue->unit; i++) {
		((char *)bytes)[i] = queue->buf[queue->head];
		queue->head = (queue->head + 1) % queue->size;
	}

	return 1;
}
