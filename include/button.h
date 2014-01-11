/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Button API for Chrome EC */

#ifndef __CROS_EC_BUTTON_H
#define __CROS_EC_BUTTON_H

#include "common.h"
#include "gpio.h"

enum button_type {
	BUTTON_POWER = 0,
	BUTTON_VOLUME_DOWN,
	BUTTON_VOLUME_UP,
	BUTTON_COUNT
};

enum button_active_type {
	BUTTON_ACTIVE_HIGH = 0,
	BUTTON_ACTIVE_LOW,
};

struct button_config {
	const char *name;
	enum button_type type;
	enum gpio_signal gpio;
	enum button_active_type active_level;
	uint32_t debounce_us;
};

/*
 * Defined in board.c.
 */
extern const struct button_config buttons[];

/*
 * Interrupt handler for button.
 *
 * @param signal	Signal which triggered the interrupt.
 */
void button_interrupt(enum gpio_signal signal);

/*
 * Respond to button changes. Implemented by a host-specific
 * handler.
 *
 * @param button	The button that changed.
 * @param is_pressed	Whether the button is now pressed.
 */
void button_state_changed(enum button_type button, int is_pressed);

#endif  /* __CROS_EC_BUTTON_H */
