/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console_util.h"
#include "sim_timer.h"
#include "sim_persist.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fcb.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_DECLARE(sim, LOG_LEVEL_DBG);


static const char *timer_path = "sim/timers/disconnection";
const struct timer_state timer_default = {
	.disconnection = 1000,
	.button_response_delay = 2000,
};


static struct timer_state state;

void sim_timer_init(void)
{
	state = timer_default;
	struct storage_result result = INIT_STORAGE_RESULT(state.disconnection);
	bool valid = sim_persist_load(timer_path, &result);
	if (!valid) {
		state.disconnection = timer_default.disconnection;
	}
}

const struct timer_state *sim_timer_state(void)
{
	return &state;
}

void sim_timer_set(int32_t timeout, bool reset, bool store)
{
	if (reset) {
		state.disconnection = timer_default.disconnection;
	} else {
		state.disconnection = timeout;
	}
	if (store) {
		if (!reset) {
			SIM_SAVE_FIELD(timer_path, state.disconnection);
		} else {
			SIM_ERASE_FIELD(timer_path);
		}
	}
}
