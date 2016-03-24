/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lid switch API for Chrome EC */

#ifndef __CROS_EC_LID_SWITCH_H
#define __CROS_EC_LID_SWITCH_H

#include "common.h"

int set_force_lid_open(int open);
/*
 * Allow a forced lid open for special scenarios
 *
 * @param open 		Set to 1 for force lid open
 */

int force_lid_open(int open);

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
