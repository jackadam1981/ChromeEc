/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_INCLUDE_DRIVERS_PD_CHIP_H
#define __ZEPHYR_INCLUDE_DRIVERS_PD_CHIP_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ccg8_driver_api {
	int (*pd_intel_retimer_fw_update)(const struct device *dev,
					  bool enable);
};

/**
 * @brief Command CCG8 PD chip to enter retimer firmware update mode.
 *
 * @param enable True->entry, False->exit retimer firmware update mode.
 *
 * @retval 0 if success or I2C error.`
 * @retval -EIO if input/output error.
 */
__syscall int pd_intel_retimer_fw_update(const struct device *dev, bool enable);

static inline int z_impl_pd_intel_retimer_fw_update(const struct device *dev,
						    bool enable)
{
	const struct ccg8_driver_api *api =
		(const struct ccg8_driver_api *)dev->api;

	return api->pd_intel_retimer_fw_update(dev, enable);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#include <syscalls/pd_chip.h>

#endif /* __ZEPHYR_INCLUDE_DRIVERS_PD_CHIP_H */
