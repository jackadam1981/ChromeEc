/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief GPIO names support
 * Declares the functions to retrieve the mapping between
 * gpio-line-names and the associated GPIO ports and pins.
 */

#ifndef ZEPHYR_INCLUDE_SHELL_GPIO_NAME_H_
#define ZEPHYR_INCLUDE_SHELL_GPIO_NAME_H_

#include <drivers/gpio.h>

/**
 * @brief Retrieve the GPIO port and pin associated with a name.
 *
 * @param name Pointer to the GPIO name to search for.
 * @param pport Pointer to store the GPIO port found.
 * @param ppin Pointer to store the GPIO pin found.
 *
 * @return 0 If successful.
 * @retval -ENOENT No GPIO was found with this name.
 */
int gpio_pin_by_name(const char *name,
		     const struct device **pport,
		     gpio_pin_t *ppin);

/**
 * @brief Retrieve the name associated with a GPIO port and pin.
 *
 * @param port Pointer to the GPIO device.
 * @param pin Pin number of the GPIO.
 *
 * @return Pointer to name associated with GPIO.
 * @retval NULL No name was found associated this GPIO.
 */
const char *gpio_pin_get_name(const struct device *port, gpio_pin_t pin);

/**
 * @brief Retrieve the name associated with a GPIO spec.
 *
 * @param spec Pointer to the GPIO spec..
 *
 * @return Pointer to name associated with GPIO.
 * @retval NULL No name was found associated this GPIO.
 */
static inline const char *
	gpio_pin_get_name_dt(const struct gpio_dt_spec *spec)
{
	return gpio_pin_get_name(spec->port, spec->pin);
}

#endif /* ZEPHYR_INCLUDE_SHELL_GPIO_NAME_H_ */
