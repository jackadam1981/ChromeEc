/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test queue.
 */

#include "common.h"
#include "console.h"
#include "queue.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#define QUEUE_SIZE 6 /* Max 5 items in queue */
static char buffer[QUEUE_SIZE];
static struct queue test_queue = {
	.buf_bytes  = sizeof(buffer),
	.unit_bytes = sizeof(char),
	.buf        = buffer,
};

#define LOOP_DEQUE(q, d, n) \
	do { \
		int i; \
		for (i = 0; i < n; ++i) \
			TEST_ASSERT(queue_remove_unit(&q, d + i)); \
	} while (0)

static int test_queue_empty(void)
{
	char dummy = 1;

	queue_reset(&test_queue);
	TEST_ASSERT(queue_is_empty(&test_queue));
	queue_add_units(&test_queue, &dummy, 1);
	TEST_ASSERT(!queue_is_empty(&test_queue));

	return EC_SUCCESS;
}

static int test_queue_reset(void)
{
	char dummy = 1;

	queue_reset(&test_queue);
	queue_add_units(&test_queue, &dummy, 1);
	queue_reset(&test_queue);
	TEST_ASSERT(queue_is_empty(&test_queue));

	return EC_SUCCESS;
}

static int test_queue_fifo(void)
{
	char buf1[3] = {1, 2, 3};
	char buf2[3];

	queue_reset(&test_queue);

	queue_add_units(&test_queue, buf1 + 0, 1);
	queue_add_units(&test_queue, buf1 + 1, 1);
	queue_add_units(&test_queue, buf1 + 2, 1);

	LOOP_DEQUE(test_queue, buf2, 3);
	TEST_ASSERT_ARRAY_EQ(buf1, buf2, 3);

	return EC_SUCCESS;
}

static int test_queue_multiple_units_add(void)
{
	char buf1[5] = {1, 2, 3, 4, 5};
	char buf2[5];

	queue_reset(&test_queue);
	TEST_ASSERT(queue_has_space(&test_queue, 5));
	queue_add_units(&test_queue, buf1, 5);
	LOOP_DEQUE(test_queue, buf2, 5);
	TEST_ASSERT_ARRAY_EQ(buf1, buf2, 5);

	return EC_SUCCESS;
}

static int test_queue_removal(void)
{
	char buf1[5] = {1, 2, 3, 4, 5};
	char buf2[5];

	queue_reset(&test_queue);
	queue_add_units(&test_queue, buf1, 5);
	/* 1, 2, 3, 4, 5 */
	LOOP_DEQUE(test_queue, buf2, 3);
	TEST_ASSERT_ARRAY_EQ(buf1, buf2, 3);
	/* 4, 5 */
	queue_add_units(&test_queue, buf1, 2);
	/* 4, 5, 1, 2 */
	TEST_ASSERT(queue_has_space(&test_queue, 1));
	TEST_ASSERT(!queue_has_space(&test_queue, 2));
	LOOP_DEQUE(test_queue, buf2, 1);
	TEST_ASSERT(buf2[0] == 4);
	/* 5, 1, 2 */
	queue_add_units(&test_queue, buf1 + 2, 2);
	/* 5, 1, 2, 3, 4 */
	TEST_ASSERT(!queue_has_space(&test_queue, 1));
	LOOP_DEQUE(test_queue, buf2, 1);
	TEST_ASSERT(buf2[0] == 5);
	LOOP_DEQUE(test_queue, buf2, 4);
	TEST_ASSERT_ARRAY_EQ(buf1, buf2, 4);
	TEST_ASSERT(queue_is_empty(&test_queue));
	/* Empty */
	queue_add_units(&test_queue, buf1, 5);
	LOOP_DEQUE(test_queue, buf2, 5);
	TEST_ASSERT_ARRAY_EQ(buf1, buf2, 5);

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_queue_empty);
	RUN_TEST(test_queue_reset);
	RUN_TEST(test_queue_fifo);
	RUN_TEST(test_queue_multiple_units_add);
	RUN_TEST(test_queue_removal);

	test_print_result();
}
