/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file drivers/cros_flash/cros_flash_mchp_wp.c
 * @brief Public APIs for microchip write protection.
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_MICROCHIP_WP_H_
#define ZEPHYR_INCLUDE_DRIVERS_MICROCHIP_WP_H_

/**
 * Sync up gpio_wp state with mchp_wp state. Update GPIO076 state according to
 * gpio_wp state set from SOC.
 *
 * @return none.
 */
void sync_wp_assert_status(void);

/**
 * Enable the mchp_wp gpio to allow for flash protecting.
 *
 * @return none.
 */
void enable_mchp_wp(void);

/**
 * Get the current value of the mchp_wp internal gpio.
 *
 * @return int value of the gpio.
 */
int get_mchp_wp_gpio();

#endif
