/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lid switch API for Chrome EC */

#ifndef __CROS_EC_LID_SWITCH_H
#define __CROS_EC_LID_SWITCH_H

/* Between these angles, we consider the lid is open. We define
 * them before common.h to allow board.h to overwride them. */
#define LID_ANGLE_CLOSED_LOW	4
#define LID_ANGLE_CLOSED_HIGH	356

#include "common.h"

/**
 * Return non-zero if lid is open.
 *
 * Uses the debounced lid state, not the raw signal from the GPIO.
 */
int lid_is_open(void);

/**
 * Interrupt handler for lid switch.
 *
 * @param signal	Signal which triggered the interrupt.
 */
void lid_interrupt(enum gpio_signal signal);

#endif  /* __CROS_EC_LID_SWITCH_H */
