/* Copyright 2017 The Chromium OS Authors. All rights reserved
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for MEC17xx processor
 */
/** @file gpio_chip.h
 *MEC17xx GPIO module
 */
/** @defgroup MEC17xx gpio
 */

#ifndef _GPIO_CHIP_H
#define _GPIO_CHIP_H

#include <stdint.h>
#include <stddef.h>

#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Place any C interfaces here */

int gpio_power_off(enum gpio_signal signal);

void gpio_power_off_by_mask(uint32_t port, uint32_t mask);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _GPIO_CHIP_H */
/**   @}
 */

