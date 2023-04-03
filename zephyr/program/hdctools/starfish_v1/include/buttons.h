/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_BUTTON_H__
#define __CROS_EC_STARFISH_BUTTON_H__

#include "gpio_helper.h"

#include <zephyr/kernel.h>

enum BUTTON_STATE {
	/* Button is cleared and is not pressed. */
	BUTTON_INACTIVE,
	/* Button is currently being pressed. */
	BUTTON_HELD,
	/* Marker to identify pending events. */
	BUTTON_PENDING_EVENT_START,
	/* Button has a short press event. */
	BUTTON_SHORT = BUTTON_PENDING_EVENT_START,
	/* Button has a long press event. */
	BUTTON_LONG,
};

#endif /* __CROS_EC_STARFISH_BUTTON_H__ */
