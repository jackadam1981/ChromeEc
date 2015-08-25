/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test queue printing.
 */

#include "common.h"
#include "console.h"
#include "consumer_printing.h"
#include "queue_policies.h"
#include "test_util.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>

/*****************************************************************************
 * Build a test consumer that just records calls to written and flush.  Calls
 * to flush empty the queue associated with the consumer.
 */
struct test_state {
	size_t written;

	int flush;
};

struct test_consumer {
	struct test_state *state;

	struct consumer consumer;

	struct queue *buffer;
};

static void test_consumer_written(struct consumer const *consumer,
				  size_t count)
{
	struct test_consumer const *test_consumer =
		DOWNCAST(consumer, struct test_consumer, consumer);

	test_consumer->state->written += count;
}

static void test_consumer_flush(struct consumer const *consumer)
{
	struct test_consumer const *test_consumer =
		DOWNCAST(consumer, struct test_consumer, consumer);

	test_consumer->state->flush++;

	assert(queue_advance_head(consumer->queue,
				  queue_count(consumer->queue)));
}

static struct consumer_ops test_consumer_ops = {
	.written = test_consumer_written,
	.flush   = test_consumer_flush,
};


#define TEST_CONSUMER(QUEUE)				\
	((struct test_consumer) {			\
		.state = &((struct test_state) {	\
			.written = 0,			\
			.flush   = 0,			\
		}),					\
		.consumer = {				\
			.queue = &QUEUE,		\
			.ops   = &test_consumer_ops,	\
		},					\
		.buffer = &QUEUE_NULL(16, char),	\
	})

/*****************************************************************************/

static struct test_consumer const consumer =
	TEST_CONSUMER(QUEUE_DIRECT(8,
				   char,
				   null_producer,
				   consumer.consumer));

static void consumer_initialize(struct test_consumer const *consumer)
{
	queue_init(consumer->consumer.queue);

	consumer->state->written = 0;
	consumer->state->flush   = 0;
}

static int check_queue_contents(struct test_consumer const *consumer,
				char const *string)
{
	size_t i;

	for (i = 0; string[i] != 0; ++i) {
		char character;

		TEST_ASSERT(queue_remove_unit(consumer->consumer.queue,
					      &character) == 1);
		TEST_ASSERT(character == string[i]);
	}

	return EC_SUCCESS;
}

/*****************************************************************************/

static int test_consumer_printf(void)
{
	char const message[] = "01234567";

	consumer_initialize(&consumer);

	TEST_ASSERT(consumer_printf(&consumer.consumer, 0, "%s", message) == 0);

	TEST_ASSERT(consumer.state->written != 0);
	TEST_ASSERT(consumer.state->flush   == 0);
	TEST_CHECK(check_queue_contents(&consumer, message) == EC_SUCCESS);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_consumer_printf);

	test_print_result();
}
