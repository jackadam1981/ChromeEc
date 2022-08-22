/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "include/lpc.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

struct console_cmd_hostevent {
	host_event_t lpc_host_events;
	host_event_t lpc_host_event_mask[LPC_HOST_EVENT_COUNT];
};

static void *console_cmd_hostevent_setup(void)
{
	static struct console_cmd_hostevent fixture = { 0 };

	return &fixture;
}

static void console_cmd_hostevent_before(void *fixture)
{
	struct console_cmd_hostevent *f = fixture;

	/* Save all current events and masks */
	f->lpc_host_events = lpc_get_host_events();

	for (int i = 0; i < LPC_HOST_EVENT_COUNT; i++) {
		f->lpc_host_event_mask[i] = lpc_get_host_events_by_type(i);
	}
}

static void console_cmd_hostevent_after(void *fixture)
{
	struct console_cmd_hostevent *f = fixture;

	/* Restore all current events and masks */
	lpc_set_host_event_state(f->lpc_host_events);

	for (int i = 0; i < LPC_HOST_EVENT_COUNT; i++) {
		lpc_set_host_event_mask(i, f->lpc_host_event_mask[i]);
	}
}

/* hostevent with no arguments */
ZTEST_USER(console_cmd_hostevent, test_hostevent)
{
	zassert_ok(shell_execute_cmd(get_ec_shell(), "hostevent"),
		   "Failed default print");
}

/* hostevent with invalid arguments */
ZTEST_USER(console_cmd_hostevent, test_hostevent_invalid)
{
	int rv;

	/* Test invalid sub-command */
	rv = shell_execute_cmd(get_ec_shell(), "hostevent invalid 0xFFFF");
	zassert_equal(rv, EC_ERROR_PARAM1, "Expected %d, but got %d",
		      EC_ERROR_PARAM1, rv);

	/* Test invalid mask */
	rv = shell_execute_cmd(get_ec_shell(), "hostevent set invalid-mask");
	zassert_equal(rv, EC_ERROR_PARAM2, "Expected %d, but got %d",
		      EC_ERROR_PARAM2, rv);
}

ZTEST_SUITE(console_cmd_hostevent, drivers_predicate_post_main,
	    console_cmd_hostevent_setup, console_cmd_hostevent_before,
	    console_cmd_hostevent_after, NULL);
