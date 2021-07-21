/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_TEST_DRIVERS_INCLUDE_TEST_FRAMEWORK_H
#define __ZEPHYR_TEST_DRIVERS_INCLUDE_TEST_FRAMEWORK_H

#include <stdbool.h>
#include <ztest.h>

/**
 * A single node used to hold a test suite in a linked list. These will be
 * chained together to create the full set of tests to be run during
 * test_main().
 */
struct test_node {
	/** The name of the test suite */
	const char *name;
	/** Pointer to the first test in the suite */
	struct unit_test *suite;
	/** Whether or not this suite should be run before ec_app_main() */
	bool before_main;
	/** The next node in the list of test suites */
	struct test_node *next;
};

/** The head of the test suites' linked list. */
extern struct test_node *test_head;

/**
 * Register a new test suite to be run.
 *
 * @param SUITE_NAME The name of the test suite.
 * @param BEFORE_MAIN True if the suite should be run before ec_app_main, false
 * otherwise.
 */
#define register_test_suite(SUITE_NAME, BEFORE_MAIN, ...)             \
	ztest_test_suite(SUITE_NAME, __VA_ARGS__);                    \
	static struct test_node test_node_##SUITE_NAME = {            \
		.name = #SUITE_NAME,                                  \
		.suite = _##SUITE_NAME,                               \
		.before_main = (BEFORE_MAIN),                         \
	};                                                            \
	static int test_suite_##name##_init(const struct device *dev) \
	{                                                             \
		ARG_UNUSED(dev);                                      \
		struct test_node *old_head = test_head;               \
		test_head = &test_node_##SUITE_NAME;                  \
		test_node_##SUITE_NAME.next = old_head;               \
		return 0;                                             \
	}                                                             \
	SYS_INIT(test_suite_##name##_init, APPLICATION, 99)

#endif /* __ZEPHYR_TEST_DRIVERS_INCLUDE_TEST_FRAMEWORK_H */
