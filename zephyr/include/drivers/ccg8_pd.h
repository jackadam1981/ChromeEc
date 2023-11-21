/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_CAPCTRL_REG_PD_P0 0x105C
#define HOST_CAPCTRL_REG_PD_P1 0x205C
#define INTEL_HOSTCAP_CTRL_PD_REG_LEN 1

#define PD_ICL_BB_RETIMER_CMD_REG 0x0046
#define PD_ICL_BB_RETIMER_CMD_REG_LEN 2
#define PD_ICL_CTRL_REG 0x0040
#define PD_ICL_CTRL_REG_LEN 1

extern const struct device *pd_pow_config_array[];

struct ccg8_driver_api {
	int (*pd_write_powmode)(const struct device *dev, uint16_t reg,
				uint8_t len, void *data);
	int (*pd_read_powmode)(const struct device *dev, uint16_t reg,
			       uint8_t len, void *buf);
};

__syscall int pd_write_powmode(const struct device *dev, uint16_t reg,
			       uint8_t len, void *data);
__syscall int pd_read_powmode(const struct device *dev, uint16_t reg,
			      uint8_t len, void *buf);

static inline int z_impl_pd_write_powmode(const struct device *dev,
					  uint16_t reg, uint8_t len, void *data)
{
	const struct ccg8_driver_api *api =
		(const struct ccg8_driver_api *)dev->api;

	return api->pd_write_powmode(dev, reg, len, data);
}

static inline int z_impl_pd_read_powmode(const struct device *dev, uint16_t reg,
					 uint8_t len, void *buf)
{
	const struct ccg8_driver_api *api =
		(const struct ccg8_driver_api *)dev->api;

	return api->pd_read_powmode(dev, reg, len, buf);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#include <syscalls/ccg8_pd.h>
