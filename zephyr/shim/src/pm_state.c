/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <stdint.h>

#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>

static void pm_policy_state_for_each(void (*fn)(enum pm_state, uint8_t))
{
	const struct pm_state_info *cpu_states;
	uint8_t num_cpu_states = pm_state_cpu_get_all(0U, &cpu_states);

	for (uint8_t i = 0; i < num_cpu_states; ++i) {
		fn(cpu_states[i].state, PM_ALL_SUBSTATES);
	}
}

void pm_policy_state_lock_get_all(void)
{
	pm_policy_state_for_each(pm_policy_state_lock_get);
}

void pm_policy_state_lock_put_all()
{
	pm_policy_state_for_each(pm_policy_state_lock_put);
}
