/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Input devices using Matrix Keyboard Protocol [MKBP] events for Chrome EC */

#pragma once

#include "common.h"
#include "ec_commands.h"

/**
 * Update the state of the switches.
 *
 * @param sw		The switch that changed.
 * @param state		The state of the switch.
 */
void mkbp_update_switches(uint32_t sw, int state);


/**
 * Update the state of buttons
 *
 * @param button	The button that changed.
 * @param is_pressed	Whether the button is now pressed.
 */
void mkbp_button_update(enum keyboard_button_type button, int is_pressed);

/**
 * Retrieve state of buttons [Power, Volume up/down, etc]
 */
uint32_t get_mkbp_button_state(void);

/**
 * Retrieve state of switches [Lid open/closed, tablet mode switch, etc]
 */
uint32_t get_mkbp_switch_state(void);
