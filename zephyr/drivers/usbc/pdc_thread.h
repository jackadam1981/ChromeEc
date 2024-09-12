/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_USBC_PDC_THREAD_H
#define ZEPHYR_DRIVERS_USBC_PDC_THREAD_H

#include <zephyr/device.h>

int pdc_thread_init();

#define pdc_thread_post_msg(dev, event) \
	pdc_thread_post_msg_(dev, event, __func__)

int pdc_thread_post_msg_(const struct device *dev, uint32_t event,
			 const char *caller);

#endif /* ZEPHYR_DRIVERS_USBC_PDC_THREAD_H */
