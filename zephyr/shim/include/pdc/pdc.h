/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include <zephyr/device.h>

/**
 * @brief Get a PDC device struct pointer by port number
 *
 * @param port Port number (listed in the reg prop in nodes under
 *        named-usbc-port)
 * @return Pointer to device struct for associated PDC, or NULL if not found.
 */
const struct device *
pdc_get_device_for_port(const int port);

/**
 * @brief Get legacy I2C port enum for a given USB-C port's PDC
 *
 * @param i2c_dev Zephyr device struct pointer for the target I2C port
 * @return i2c_ports enum if match is found, or -1 if not.
*/
enum i2c_ports pdc_get_legacy_i2c_port_for_port(const int port);

/**
 * @brief Get the number of PDC devices instantiated
 */
const size_t pdc_get_device_count(void);
