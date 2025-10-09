/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board buttons */

#ifndef __BOARD_BUTTONS_H
#define __BOARD_BUTTONS_H

#include "ec_commands.h"
#include "gpio_signal.h"

enum board_button {
	BUTTON_CAMERA,
	BUTTON_BACK,
	BUTTON_OVERVIEW,
	BUTTON_LAUNCHER,
	BOARD_BUTTON_COUNT,
};

struct board_button_config {
	const char *name;
	enum keyboard_button_type type;
	enum gpio_signal gpio;
	uint32_t debounce_us;
	int flags;
};

extern const struct board_button_config board_buttons[];

/*
 * Interrupt handler for button.
 *
 * @param signal	Signal which triggered the interrupt.
 */
void board_button_interrupt(enum gpio_signal signal);

#endif /* __BOARD_BUTTONS_H */
