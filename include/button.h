/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Button API for Chrome EC */

#ifndef __CROS_EC_BUTTON_H
#define __CROS_EC_BUTTON_H

enum button_type {
	BUTTON_POWER = 0,
	BUTTON_VOLUME_DOWN,
	BUTTON_VOLUME_UP,
	BUTTON_COUNT
};

/*
 * Respond to button changes. Implemented by a host-specific
 * handler.
 *
 * @param button	The button that changed.
 * @param is_pressed	Whether the button is now pressed.
 */
void button_state_changed(enum button_type button, int is_pressed);

#endif  /* __CROS_EC_BUTTON_H */
