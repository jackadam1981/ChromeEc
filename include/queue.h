/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Queue data structure.
 */

#include <stdint.h>

/* Generic queue container.
 *
 *   head: next to dequeqe
 *   tail: next to enqueue
 *
 *   Empty:
 *     head == tail
 *   Full:
 *     tail + 1 == head
 */
struct queue {
	int head, tail;
	int size;      /* size of buffer (in byte) */
	int unit;      /* size of unit (in byte) */
	uint8_t *buf;
};

void queue_reset(struct queue *queue);
int queue_empty(struct queue *queue);
int queue_has_space(struct queue *queue, int count);
void queue_enqueue(struct queue *queue, const void *bytes, const int count);
int queue_dequeue_one(struct queue *queue, void *bytes);
