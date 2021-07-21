/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include "ec_app_main.h"
#include "test_framework.h"

struct test_node *test_head;

static void execute_tests(bool before_main)
{
	struct test_node *node = test_head;

	while (node) {
		/* Skip tests that don't match current condition. */
		if (node->before_main == before_main)
			z_ztest_run_test_suite(node->name, node->suite);
		node = node->next;
	}
}

void test_main(void)
{
	/* Test suites to run before ec_app_main.*/
	execute_tests(true);

	ec_app_main();

	/* Test suites to run after ec_app_main.*/
	execute_tests(false);
}
