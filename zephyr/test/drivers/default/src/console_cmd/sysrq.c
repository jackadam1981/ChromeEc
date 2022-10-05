/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "console.h"
#include "mkbp_fifo.h"
#include "test/drivers/test_state.h"
#include "system.h"

ZTEST_USER(console_cmd_sysrq, test_no_args)
{
	const struct shell *shell_zephyr = get_ec_shell();
	/* Size of SYSRQ event */
	uint32_t out = 0;

	mkbp_clear_fifo();

	shell_backend_dummy_clear_output(shell_zephyr);
	k_sleep(K_SECONDS(1));

	/* Send arbitrary key 'q' */
	zassert_ok(shell_execute_cmd(shell_zephyr, "sysrq q"), NULL);

	int event_size =
		mkbp_fifo_get_next_event((uint8_t *)&out, EC_MKBP_EVENT_SYSRQ);

	zassert_equal(event_size, sizeof(uint32_t));
	zassert_equal(out, 'q');
}

ZTEST_SUITE(console_cmd_sysrq, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
