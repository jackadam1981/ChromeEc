/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define GPIO_KB_INPUT  (GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_BOTH)
#define GPIO_KB_OUTPUT GPIO_ODR_HIGH

#include "gpio.h"
#include "registers.h"
#include "util.h"


/* To define the gpio_list[] instance. */
#include "gpio_list.h"

