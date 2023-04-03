/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_TIMER_H__
#define __CROS_EC_STARFISH_SIM_TIMER_H__

#include <stdint.h>

struct timer_state {
	/* Remember the last SIM card */
	int32_t disconnection;
	/* Delay before responding to button presses. */
	int32_t button_response_delay;
};

void sim_timer_init(void);

const struct timer_state *sim_timer_state(void);

void sim_timer_set(int32_t timeout, bool reset, bool store);

#endif /* __CROS_EC_STARFISH_SIM_TIMER_H__ */
