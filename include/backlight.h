/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Backlight API for Chrome EC */

#ifndef __CROS_EC_BACKLIGHT_H
#define __CROS_EC_BACKLIGHT_H

#include "common.h"
#include "gpio.h"

/**
 * parameter for board to decide the timing of en/disable backlight.
 * 0: default, update_backlight is called in HOOK_INIT
 * 1: call enable_backlight in board level.
 */
extern int board_update_backlight;

/**
 * Interrupt handler for backlight.
 *
 * @param signal	Signal which triggered the interrupt.
 */
#ifdef CONFIG_BACKLIGHT_REQ_GPIO
void backlight_interrupt(enum gpio_signal signal);
#else
static inline void backlight_interrupt(enum gpio_signal signal)
{
}
#endif /* !CONFIG_BACKLIGHT_REQ_GPIO */

/**
 * Activate/Deactivate the backlight GPIO pin considering active high or low.
 */
void enable_backlight(int enabled);

#endif /* __CROS_EC_BACKLIGHT_H */
