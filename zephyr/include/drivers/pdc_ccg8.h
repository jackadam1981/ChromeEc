/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CCG8_PD
#define __CROS_EC_CCG8_PD

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PD_ICL_CTRL_REG 0x0040
#define PD_ICL_CTRL_REG_LEN 1

extern const struct device *pd_pow_config_array[];

struct ccg8_driver_api {
	int (*pd_intel_retimer_fw_update)(const struct device *dev,
					bool enable);
};

/**
 * @brief Command CCG8 PD chip to enter retimer firmware update mode.
 * @param enable True->entry, False->exit retimer firmware update mode.
 */
__syscall int pd_intel_retimer_fw_update(const struct device *dev,
					bool enable);

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
#include <syscalls/pdc_ccg8.h>

#endif /* __CROS_EC_CCG8_PD */
