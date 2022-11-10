/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h> /* nocheck */
#include <zephyr/ztest.h>

#include "console.h"
#include "task.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

ZTEST_SUITE(tasks, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(tasks, test_enable_irq)
{
	arch_irq_disable(0);
	task_enable_irq(0);
	zassert_true(arch_irq_is_enabled(0));
}

ZTEST(tasks, test_taskinfo)
{
	static char buffer[4000];
	const char *shell_buffer;
	size_t shell_buffer_size;

	shell_backend_dummy_clear_output(get_ec_shell());
	zassert_ok(shell_execute_cmd(get_ec_shell(), "taskinfo"));

	k_msleep(100);
	shell_buffer = shell_backend_dummy_get_output(get_ec_shell(),
						      &shell_buffer_size);

	zassert_true(
		shell_buffer_size < ARRAY_SIZE(buffer),
		"Not enough memory, please allocate more memory for the buffer");
	memcpy(buffer, shell_buffer, shell_buffer_size);
	printk("got:\n%s\n\n", buffer);
}
