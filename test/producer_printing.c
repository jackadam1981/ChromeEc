/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test queue printing.
 */

#include "common.h"
#include "console.h"
#include "producer_printing.h"
#include "queue_policies.h"
#include "test_util.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>

/*****************************************************************************
 * Build a test consumer that just records the number of bytes written.
 */
struct test_state {
	size_t written;
};

struct test_consumer {
	struct test_state *state;

	struct consumer consumer;
};

static void test_consumer_written(struct consumer const *consumer,
				  size_t count)
{
	struct test_consumer const *test_consumer =
		DOWNCAST(consumer, struct test_consumer, consumer);

	test_consumer->state->written += count;
}

static struct consumer_ops test_consumer_ops = {
	.written = test_consumer_written,
	.flush   = NULL,
};

#define TEST_CONSUMER(QUEUE)				\
	((struct test_consumer) {			\
		.state = &((struct test_state) {	\
			.written = 0,			\
		}),					\
		.consumer = {				\
			.queue = &QUEUE,		\
			.ops   = &test_consumer_ops,	\
		},					\
	})

/*****************************************************************************
 * The test framework consists of a queue, a non-blocking printf producer
 * feeding the queue, and a test_consumer reading the queue.
 */

struct test
{
	struct queue           queue;
	struct test_consumer   consumer;
	struct printf_producer producer;
};

#define TEST_CONSTRUCT(THIS, SIZE)					\
	((struct test) {						\
		.queue    = QUEUE_DIRECT(SIZE,				\
					 char,				\
					 THIS.producer.producer,	\
					 THIS.consumer.consumer),	\
		.consumer = TEST_CONSUMER(THIS.queue),			\
		.producer = PRINTF_PRODUCER(THIS.queue),		\
	})

static int check_queue_contents(struct queue const *queue, char const *string)
{
	while (*string) {
		char character;

		TEST_ASSERT(queue_remove_unit(queue, &character) == 1);
		TEST_ASSERT(character == *string++);
	}

	return EC_SUCCESS;
}

/*****************************************************************************/

static int test_producer_printf(void)
{
	const char message[]   = "01234567";
	const struct test test = TEST_CONSTRUCT(test, 8);

	TEST_ASSERT(printf_producer_printf(&test.producer, "%s", message) == 0);

	TEST_ASSERT(test.consumer.state->written != 0);
	TEST_CHECK(check_queue_contents(&test.queue, message) == EC_SUCCESS);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_producer_printf);

	test_print_result();
}
