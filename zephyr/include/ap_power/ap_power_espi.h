/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief API for power signal ESPI callback.
 */

#ifndef __AP_POWER_AP_POWER_ESPI_H__
#define __AP_POWER_AP_POWER_ESPI_H__

#include <zephyr/drivers/espi.h>

/**
 * @brief ESPI callback.
 *
 * @param dev ESPI device
 * @param cb Callback structure
 * @param event ESPI event data
 */
void power_signal_espi_cb(const struct device *dev, struct espi_callback *cb,
			  struct espi_event event);

#endif /* __AP_POWER_AP_POWER_ESPI_H__ */
