/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <zephyr/ztest.h>

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
	ARG_UNUSED(state);
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);
	irq_unlock(0);
}

/* For line coverage of the pm handler functions. */
ZTEST(system, test_pm_handlers)
{
	pm_state_set(0, 0);
	irq_lock();
	pm_state_exit_post_ops(0, 0);
}
