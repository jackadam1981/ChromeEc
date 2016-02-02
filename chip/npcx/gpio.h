/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CHIP_NPCX_GPIO_H
#define __CROS_EC_CHIP_NPCX_GPIO_H

#include "include/gpio.h"

/**
 * Lookup an alternate group/mask for a given GPIO in port/mask form
 *
 * @param gpio_port GPIO port to lookup
 * @param gpio_mask GPIO mask to lookup
 *                  Only a single bit may be set in this mask
 * @param alt_group Output parameter where alternate group is written
 *                  This must not be NULL
 *                  This is only updated if the GPIO is found
 * @param alt_mask  Output parameter where alternate mask is written
 *                  This must not be NULL
 *                  This is only updated if the GPIO is found
 *
 * @return EC_SUCCESS     if the GPIO was found
 *         EC_ERROR_INVAL if the GPIO was not found
 */
int gpio_get_alt_from_gpio(uint8_t gpio_port,
			   uint8_t gpio_mask,
			   uint8_t *alt_group,
			   uint8_t *alt_mask);

/**
 * Lookup a GPIO port/mask given its alternate group/mask
 *
 * @param alt_group Alternate group to lookup
 * @param alt_mask  Alternate mask to lookup
 *                  Only a single bit may be set in this mask
 * @param gpio_port Output parameter where GPIO port number is written
 *                  This must not be NULL
 *                  This is only updated if the alternate function is found
 * @param gpio_mask Output parameter where GPIO mask is written
 *                  This must not be NULL
 *                  This is only updated if the alternate function is found
 *
 * @return EC_SUCCESS     if the alternate function was found
 *         EC_ERROR_INVAL if the alternate function was not found
 */
int gpio_get_gpio_from_alt(uint8_t alt_group,
			   uint8_t alt_mask,
			   uint8_t *gpio_port,
			   uint8_t *gpio_mask);

#endif  /* __CROS_EC_CHIP_NPCX_GPIO_H */
