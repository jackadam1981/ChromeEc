/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common Base detection code. */

#include "adc.h"
#include "common.h"
#include "gpio.h"

enum base_detect_state {
	BASE_DETACHED = 0,
	BASE_ATTACHED_DEBOUNCE,
	BASE_ATTACHED,
	BASE_DETACHED_DEBOUNCE,
};

struct base_det_cfg {
	enum adc_channel attach_pin;
	enum adc_channel detach_pin;
};
extern const struct base_det_cfg base_pin_cfg;

/**
 * Board specific implementation to determine if a base seems attached based off
 * of the reading of the attach pin.
 *
 * @param mv: The reading of the attach pin in millivolts.
 * @return non-zero if attached, 0 otherwise.
 */
int base_seems_attached(int attach_pin_mv, int detach_pin_mv);

/**
 * Board specific implementation to determine if a base seems detached based off
 * of the reading of the detach pin.
 *
 * @param mv: The reading of the detach pin in millivolts.
 * @return non-zero if detached, 0 otherwise.
 */
int base_seems_detached(int attach_pin_mv, int detach_pin_mv);

/**
 * Returns the current base detection state.
 */
enum base_detect_state base_get_detect_state(void);
