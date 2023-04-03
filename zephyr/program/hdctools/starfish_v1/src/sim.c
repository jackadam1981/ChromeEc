/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console_util.h"
#include "sim.h"
#include "sim_button.h"
#include "sim.h"
#include "sim_timer.h"
#include "sim_gpio.h"
#include "sim_persist.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim);

void sim_task_wakup(void) {
	extern const k_tid_t sim_task_id;
	k_wakeup(sim_task_id);
}


static const char *mux_state_path = "sim/mux";
struct sim_mux_ctrl state;

static void load_state() {
	struct mux_state boot_mux;
	struct storage_result result = INIT_STORAGE_RESULT(boot_mux);
	bool valid = sim_persist_load(mux_state_path, &result);
	if (valid) {
		state.remember = true;
	} else {
		boot_mux.enabled = false;
	}
	sim_slot_change(&boot_mux, MUX_SOURCE_DIRECT);
}

static void save_state() {
	if (state.remember) {
		struct mux_state mux = state.mux_states[0].mux;
		SIM_SAVE_FIELD(mux_state_path, mux);
	}
}


static void update_mux(void)
{
	struct mux_state mux = state.mux_states[0].mux;
	write_gpio(GPIO_LABEL_SIM_HOST_EN, 0, mux.enabled);
	write_gpio(GPIO_LABEL_HOST_DET_CTRL, 0, mux.enabled);
	write_gpio(GPIO_LABEL_SIM_MUX_CLK_EN, 0, false);
	write_gpio(GPIO_LABEL_SIM_MUX_DATA_EN, 0, false);
	write_gpio(GPIO_LABEL_SIM_MUX_RST_EN, 0, false);
	for (int i = 0; i < 3; i++) {
		bool enabled = (mux.slot_number & (1 << i)) != 0;
		write_gpio(GPIO_LABEL_SIM_MUX_ADDR, i, enabled);
	}
	write_gpio(GPIO_LABEL_SIM_MUX_CLK_EN, 0, mux.enabled);
	write_gpio(GPIO_LABEL_SIM_MUX_DATA_EN, 0, mux.enabled);
	write_gpio(GPIO_LABEL_SIM_MUX_RST_EN, 0, mux.enabled);
	if (mux.enabled) {
		LOG_INF("SIM Mux set to %u", mux.slot_number);
	} else {
		LOG_INF("SIM Mux disabled");
	}
}

const struct sim_mux_ctrl *sim_slot_state(void)
{
	return &state;
}

const struct mux_state *sim_slot_get_current_mux(void)
{
	return &state.mux_states[0].mux;
}
const struct mux_state *sim_slot_get_desired_mux(void)
{
	return &state.mux_states[state.future_cnt].mux;
}

void sim_slot_save_mux_boot(bool stored) {
	state.remember = stored;
	if (stored) {
		save_state();
	} else {
		SIM_ERASE_FIELD(mux_state_path);
	}
}

uint8_t sim_slot_get_detected_sims()
{
	int result = 0;
	for (int i = 0; i < NUM_SIMS; i++) {
		if (read_gpio(GPIO_LABEL_SIM_CD, i)) {
			result |= BIT(i);
		}
	}
	return (uint8_t)result;
}


uint8_t get_adjacent_sim(uint8_t start, bool higher) {
	uint8_t detected_sims = sim_slot_get_detected_sims();
	for (int offset = 1; offset < NUM_SIMS; offset++) {
		int index = start;
		if (higher) {
			index += offset;
		} else {
			index -= offset;
		}
		if (index < 0) {
			index += NUM_SIMS;
		}
		index = index % NUM_SIMS;
		if (BIT(index) & detected_sims) {
			return (uint8_t)index;
		}
	}
	return start;
}

void sim_slot_change(const struct mux_state *new_state, enum MUX_CHANGE_SRC src)
{
	int64_t start_ts = k_uptime_get();
	if (src == MUX_SOURCE_DIRECT) {
		state.mux_states[1].mux = *new_state;
		state.mux_states[1].timestamp = start_ts;
		state.future_cnt = 1;
		return;
	}
	if (src == MUX_SOURCE_BUTTON) {
		start_ts += sim_timer_state()->button_response_delay;
	}
	if (state.mux_states[0].mux.enabled && new_state->enabled) {
		// Turn off the mux, wait, and turn it back on
		state.mux_states[1].mux = *new_state;
		state.mux_states[1].mux.enabled = false;
		state.mux_states[1].timestamp = start_ts;
		state.mux_states[2].mux = *new_state;
		state.mux_states[2].timestamp = start_ts + sim_timer_state()->disconnection;
		state.future_cnt = 2;
	} else if (!state.mux_states[0].mux.enabled && new_state->enabled) {
		// Wait to make sure the mux was off for the minimum time
		int64_t min_on = state.mux_states[0].timestamp + sim_timer_state()->disconnection;
		state.mux_states[1].mux = *new_state;
		state.mux_states[1].timestamp = MAX(start_ts, min_on);
		state.future_cnt = 1;
	} else if (state.mux_states[0].mux.enabled && !new_state->enabled) {
		// Simply need to turn off the mux
		state.mux_states[1].mux = *new_state;
		state.mux_states[1].timestamp = start_ts;
		state.future_cnt = 1;
	}
	save_state();
	sim_task_wakup();
}

int64_t sim_slot_update_delay(void)
{
	if (!state.future_cnt) {
		return -1;
	}
	int64_t now = k_uptime_get();
	int64_t delta = state.mux_states[1].timestamp - now;
	if (delta < 0) {
		return 0;
	}
	return delta;
}

void sim_slot_handle_update(void) {
	if (sim_slot_update_delay() != 0) {
		return;
	}
	state.mux_states[0] = state.mux_states[1];
	state.mux_states[1] = state.mux_states[2];
	state.future_cnt--;
	update_mux();
}


static int sim_task_init(const struct device *device)
{
	sim_gpio_init();
	sim_button_init();
	sim_timer_init();
	load_state();
	update_mux();
	return 0;
}


static void sim_task()
{
	while (true) {
		int64_t delay = sim_slot_update_delay();
		if (delay == 0) {
			sim_slot_handle_update();
			continue;
		}
		k_msleep(delay);
	}
}

K_THREAD_DEFINE(sim_task_id, 2000, sim_task, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

SYS_INIT(sim_task_init, APPLICATION, 1);

