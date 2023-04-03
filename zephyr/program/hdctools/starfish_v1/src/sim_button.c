/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console_util.h"
#include "sim_button.h"
#include "sim_persist.h"
#include "sim.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fcb.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_DECLARE(sim, LOG_LEVEL_DBG);

static struct button_ctrl state;

static const char *button_path = "sim/button";
static const struct button_ctrl state_default = {
	.enabled = false,
};

static void load_state() {
	struct button_ctrl button;
	struct storage_result result = INIT_STORAGE_RESULT(button);
	bool valid = sim_persist_load(button_path, &result);
	if (valid) {
		state = button;
	}
}

void sim_button_save(const struct button_ctrl *new_state, bool reset, bool store) {
	if (reset) {
		state = state_default;
	} else {
		state = *new_state;
	}
	if (store) {
		if (!reset) {
			SIM_SAVE_FIELD(button_path, state);
		} else {
			SIM_ERASE_FIELD(button_path);
		}
	}
}

void sim_button_init(void)
{
	state = state_default;
	load_state();
}

const struct button_ctrl *sim_button_state(void)
{
	return &state;
}

void sim_button_set(const struct button_ctrl *new_state, bool stored)
{
	state = *new_state;
}



static void process_event(enum SIM_BUTTON_EVENT event) {
	if (event == SIM_BUTTON_EVENT_MODE_LONG) {
		state.enabled = !state.enabled;
		if (state.enabled) {
			LOG_INF("SIM Buttons enabled");
		} else {
			LOG_INF("SIM Buttons disabled");
		}
		return;
	}
	if(!state.enabled) {
		LOG_ERR("Rejected: SIM Buttons disabled");
		return;
	}

	if (event == SIM_BUTTON_EVENT_MODE_SHORT) {
		struct mux_state new_state = *sim_slot_get_current_mux();
		new_state.enabled = !new_state.enabled;
		LOG_INF("SIM Button toggle %d", new_state.enabled);
		sim_slot_change(&new_state, MUX_SOURCE_BUTTON);
	} else {
		struct mux_state new_state = *sim_slot_get_desired_mux();
		new_state.enabled = true;
		new_state.slot_number =
			get_adjacent_sim(new_state.slot_number, event == SIM_BUTTON_EVENT_NEXT);
		LOG_INF("SIM Button new slot %d", new_state.slot_number);
		sim_slot_change(&new_state, MUX_SOURCE_BUTTON);
	}
}

void sim_button_event(enum GPIO_LABEL button, enum BUTTON_STATE state) {
	if (button == GPIO_LABEL_BUTTON_MODE && state == BUTTON_LONG) {
		process_event(SIM_BUTTON_EVENT_MODE_LONG);
	} else if (state == BUTTON_SHORT) {
		switch (button) {
		case GPIO_LABEL_BUTTON_MODE:
			process_event(SIM_BUTTON_EVENT_MODE_SHORT);
			break;
		case GPIO_LABEL_BUTTON_NEXT:
			process_event(SIM_BUTTON_EVENT_NEXT);
			break;
		case GPIO_LABEL_BUTTON_PREV:
			process_event(SIM_BUTTON_EVENT_PREV);
			break;
		default:
			break;
		}
	}
}
