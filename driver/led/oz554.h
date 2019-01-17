/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * O2 Micro OZ554 LED driver.
 */

#ifndef __CROS_EC_OZ554_H
#define __CROS_EC_OZ554_H

#include "gpio.h"

void oz554_board_init(void) __attribute__((weak));
void oz554_change_order(int offset, int data);

void backlight_enable_interrupt(enum gpio_signal signal);

#endif
