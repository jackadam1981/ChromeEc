/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_USBC_PDC_THREAD_H
#define ZEPHYR_DRIVERS_USBC_PDC_THREAD_H

#include <zephyr/device.h>

/**
 * @brief Initialize PDC thread
 *
 * @param dev Pointer to device
 */
void pdc_thread_init(const struct device *dev);

/**
 * @brief Post message function wrapper to populate caller
 *
 */
#define pdc_thread_post_msg(dev, event) \
	pdc_thread_post_msg_(dev, event, __func__)

/**
 * @brief Post message to PDC thread queue to be processed
 *
 * @param dev Pointer to device to handle processing
 * @param event Event to be processed
 * @param caller function name invoking this method - debugging
 * @return int 0 upon success
 */
int pdc_thread_post_msg_(const struct device *dev, uint32_t event,
			 const char *caller);

/**
 * @brief Determine if PDC thread is idle
 *
 * @return true
 * @return false
 */
bool pdc_thread_is_idle();

/**
 * @brief Wait for PDC thread to be idle
 *        Helpful for testing
 *
 * @return int 0 on success, -ETIMEOUT of failure
 */
int pdc_thread_wait_for_idle();

#endif /* ZEPHYR_DRIVERS_USBC_PDC_THREAD_H */
