/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "multistep_test.h"
#include "system.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(libc_exit, LOG_LEVEL_INF);

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACK_SIZE)
static struct k_thread tdata;
static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);

static ZTEST_BMEM int execute_flag;

static void thread_call_exit(void *p1, void *p2, void *p3)
{
	execute_flag = 1;
	LOG_INF("Calling exit.\n");
	cflush();

	exit(1);
	/* Should never reach this. */
	execute_flag = 2;
	zassert_true(1 == 0);
}

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	printk("Caught system error -- reason %d\n", reason);
	printk("This is Firas.\n");
	zassert_equal(reason, K_ERR_KERNEL_PANIC);
	zassert_true(execute_flag == 1);
	printk("This is Firas again.\n");
}

ZTEST_USER(libc_exit, test1_non_essential_thread)
{
}

static void libc_exit_test(void)
{
	/* Testing with non-essential thread. */
	LOG_INF("I am in non_essential_thread.\n");
	cflush();
	execute_flag = 0;
	k_thread_create(&tdata, tstack, STACK_SIZE, thread_call_exit, NULL,
			NULL, NULL, 0, K_USER, K_NO_WAIT);
	k_msleep(100);
	/**TESTPOINT: spawned thread executed but abort itself*/
	LOG_INF("I am in non_essential_thread before assert.\n");
	cflush();
	zassert_true(execute_flag == 1);

	/* Testing with essential thread. The system should reboot. */
	LOG_INF("I am in essential_thread.\n");
	cflush();
	execute_flag = 0;

	ztest_set_fault_valid(true);
	k_thread_create(&tdata, tstack, STACK_SIZE, thread_call_exit, NULL,
			NULL, NULL, 0, K_ESSENTIAL, K_NO_WAIT);
	k_msleep(100);
	/* Should never reach this. */
	execute_flag = 2;
	zassert_true(1 == 0);
}

static void test_panic_data(void)
{
	LOG_INF("I am checking the test_panic_data.\n");
	cflush();
	zassert_true(execute_flag == 0);
}

static void (*test_steps[])(void) = { libc_exit_test, test_panic_data };

MULTISTEP_TEST(libc_exit, test_steps)
