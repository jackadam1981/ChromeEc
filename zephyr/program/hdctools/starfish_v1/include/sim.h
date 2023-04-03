/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_H__
#define __CROS_EC_STARFISH_SIM_H__

#include <stdint.h>

#define NUM_SIMS (8)

enum MUX_CHANGE_SRC {
    MUX_SOURCE_DIRECT,
    MUX_SOURCE_CONSOLE,
    MUX_SOURCE_BUTTON,
};

struct mux_state {
    /* 0-7 indicates the slot number */
    uint8_t slot_number;
    /* Remember the last SIM card */
    bool enabled;
};

struct mux_state_ts {
    struct mux_state mux;
    /* Timestamp of a state, negative indicates invalid. */
    int64_t timestamp;
};

struct sim_mux_ctrl {
    /* Current sim mux state */
    struct mux_state_ts mux_states[3];
    /* Remember the last SIM card when rebooting */
    uint8_t future_cnt;
    /* Remember the last SIM card when rebooting */
    bool remember;
};

void sim_slot_init(void);

const struct sim_mux_ctrl *sim_slot_state(void);

const struct mux_state *sim_slot_get_current_mux(void);
const struct mux_state *sim_slot_get_desired_mux(void);

void sim_slot_save_mux_boot(bool store);

uint8_t sim_slot_get_detected_sims(void);

uint8_t get_adjacent_sim(uint8_t start, bool higher);

void sim_slot_change(const struct mux_state *state, enum MUX_CHANGE_SRC src);

/**
 * Return the time until the next sim slot update.
 *
 * Returns a negative value if no updates are required.
 * @return  [description]
 */
int64_t sim_slot_update_delay(void);

void sim_slot_handle_update(void);

void sim_task_wakup(void);

#endif /* __CROS_EC_STARFISH_SIM_H__ */
