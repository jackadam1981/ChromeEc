/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* x86 power module for Chrome EC */

#ifndef __CROS_EC_X86_POWER_H
#define __CROS_EC_X86_POWER_H

#include "gpio.h"

enum x86_state {
	X86_G3 = 0,                 /*
				     * System is off (not technically all the
				     * way into G3, which means totally
				     * unpowered...)
				     */
	X86_S5,                     /* System is soft-off */
	X86_S3,                     /* Suspend; RAM on, processor is asleep */
	X86_S0,                     /* System is on */

	/* Transitions */
	X86_G3S5,                   /* G3 -> S5 (at system init time) */
	X86_S5S3,                   /* S5 -> S3 */
	X86_S3S0,                   /* S3 -> S0 */
	X86_S0S3,                   /* S0 -> S3 */
	X86_S3S5,                   /* S3 -> S5 */
	X86_S5G3,                   /* S5 -> G3 */
};

/**
 * Interrupt handler for x86 chipset GPIOs.
 */
void x86_power_interrupt(enum gpio_signal signal);

/**
 * Return system power state.
 */
enum x86_state x86_power_get_state(void);

#endif  /* __CROS_EC_X86_POWER_H */
