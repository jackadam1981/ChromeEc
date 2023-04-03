/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_BUTTON_H__
#define __CROS_EC_STARFISH_SIM_BUTTON_H__

#include "buttons.h"
#include "gpio_helper.h"

enum SIM_BUTTON_EVENT {
    SIM_BUTTON_EVENT_MODE_SHORT,
    SIM_BUTTON_EVENT_MODE_LONG,
	SIM_BUTTON_EVENT_NEXT,
	SIM_BUTTON_EVENT_PREV,
};

struct button_ctrl {
	/* Indicates if buttons are enabled or disabled. */
	bool enabled;
};

void sim_button_save(const struct button_ctrl *new_state, bool reset, bool store);

void sim_button_init(void);
const struct button_ctrl *sim_button_state(void);

void sim_button_event(enum GPIO_LABEL button, enum BUTTON_STATE state);


#endif /* __CROS_EC_STARFISH_SIM_BUTTON_H__ */
