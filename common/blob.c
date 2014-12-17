/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Handle an opaque blob of data */

#include "blob.h"
#include "common.h"
#include "console.h"
#include "printf.h"
#include "queue.h"
#include "task.h"
#include "util.h"
#include "watchdog.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

#define INCOMING_QUEUE_SIZE 128
#define OUTGOING_QUEUE_SIZE 128

static uint32_t icnt;
static uint32_t ocnt;

void blob_get_counts(uint32_t *ic, uint32_t *oc)
{
	*ic = icnt;
	*oc = ocnt;
}

static void incoming_add(struct queue_policy const *queue_policy, size_t count)
{
	icnt += count;
	task_wake(TASK_ID_BLOB);
}

static void incoming_remove(struct queue_policy const *queue_policy,
			    size_t count)
{
	ocnt += count;
#if 0
	blob_is_ready_for_more_bytes();
#endif
}

static struct queue_policy const incoming_policy = {
	.add = incoming_add,
	.remove = incoming_remove,
};

static void outgoing_add(struct queue_policy const *queue_policy, size_t count)
{
#if 0
	blob_is_ready_to_emit_bytes();
#endif
}

static void outgoing_remove(struct queue_policy const *queue_policy,
			    size_t count)
{
	/* we don't care */
}

static struct queue_policy const outgoing_policy = {
	.add = outgoing_add,
	.remove = outgoing_remove,
};

static struct queue const incoming_q = QUEUE(INCOMING_QUEUE_SIZE, uint8_t,
					     incoming_policy);

static struct queue const outgoing_q = QUEUE(OUTGOING_QUEUE_SIZE, uint8_t,
					     outgoing_policy);
size_t blob_in_space(void)
{
	return queue_space(&incoming_q);
}

size_t blob_in_count(void)
{
	return queue_count(&incoming_q);
}

size_t blob_out_space(void)
{
	return queue_space(&outgoing_q);
}

size_t blob_out_count(void)
{
	return queue_count(&outgoing_q);
}

/* Call this to send data to the blob-handler */
size_t put_bytes_to_blob(uint8_t *buffer, size_t count)
{
	return QUEUE_ADD_UNITS(&incoming_q, buffer, count);
}

size_t put_bytes_to_out_blob(uint8_t *buffer, size_t count)
{
	return QUEUE_ADD_UNITS(&outgoing_q, buffer, count);
}

/* Call this to get data back fom the blob-handler */
size_t get_bytes_from_blob(uint8_t *buffer, size_t count)
{
	return QUEUE_REMOVE_UNITS(&outgoing_q, buffer, count);
}

#define WEAK_FUNC(FOO)							\
	void __ ## FOO(void) {}						\
	void FOO(void)							\
		__attribute__((weak, alias(STRINGIFY(CONCAT2(__, FOO)))))

/* Default callbacks for outsiders */
WEAK_FUNC(blob_is_ready_for_more_bytes);
WEAK_FUNC(blob_is_ready_to_emit_bytes);


void blob_process_queue(int ctx)
{
	static uint8_t buf[INCOMING_QUEUE_SIZE];
	size_t count;

	count = blob_in_count();
	if (count == 0)
		return;

	QUEUE_REMOVE_UNITS(&incoming_q, buf, count);
	count = QUEUE_ADD_UNITS(&outgoing_q, buf, count);
}
/* Do the magic */
void blob_task(void)
{
	while (1) {
		task_wait_event(-1);
		/*
		 * Running with a high repeat count will take so long the
		 * watchdog timer fires.  So reset the watchdog timer each
		 * iteration.
		 */
		blob_process_queue(0xb);
	}
}
