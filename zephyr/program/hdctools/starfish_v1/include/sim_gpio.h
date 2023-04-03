/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_GPIO_H__
#define __CROS_EC_STARFISH_SIM_GPIO_H__

#include <stdint.h>
#include "gpio_helper.h"

/*
 * Initialize the SIM Pins
 */
void sim_gpio_init();



int write_gpio(enum GPIO_LABEL label, int idx, bool state);
int read_gpio(enum GPIO_LABEL label, int idx);

#endif /* __CROS_EC_STARFISH_SIM_GPIO_H__ */
