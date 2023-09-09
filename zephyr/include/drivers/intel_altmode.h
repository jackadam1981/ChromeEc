/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Header file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_INTEL_ALTMODE_H_
#define ZEPHYR_INCLUDE_DRIVERS_CROS_INTEL_ALTMODE_H_

#include <zephyr/device.h>

#define INTEL_ALTMODE_REG_DATA_CONTROL 0x50
#define INTEL_ALTMODE_DATA_CONTROL_REG_LEN 6

#define INTEL_ALTMODE_REG_DATA_STATUS 0x5F
#define INTEL_ALTMODE_DATA_STATUS_REG_LEN 5

typedef void (*intel_altmode_callback)(void);
typedef int (*altmode_read)(const struct device *dev, uint8_t reg,
			    uint8_t *data);
typedef int (*altmode_write)(const struct device *dev, uint8_t reg,
			     uint8_t *data);
typedef int (*altmode_isr_enable)(const struct device *dev, bool en);
typedef bool (*altmode_is_interrupted)(const struct device *dev);
typedef void (*altmode_set_result_cb)(const struct device *dev,
				      intel_altmode_callback cb);

__subsystem struct intel_altmode_driver_api {
	altmode_read read;
	altmode_write write;
	altmode_isr_enable isr_enable;
	altmode_is_interrupted is_interrupted;
	altmode_set_result_cb set_result_cb;
};

__syscall int pd_altmode_read(const struct device *dev, uint8_t reg,
			      uint8_t *data);

static inline int z_impl_pd_altmode_read(const struct device *dev, uint8_t reg,
					 uint8_t *data)
{
	const struct intel_altmode_driver_api *api =
		(const struct intel_altmode_driver_api *)dev->api;

	return api->read(dev, reg, data);
}

__syscall int pd_altmode_write(const struct device *dev, uint8_t reg,
			       uint8_t *data);

static inline int z_impl_pd_altmode_write(const struct device *dev, uint8_t reg,
					  uint8_t *data)
{
	const struct intel_altmode_driver_api *api =
		(const struct intel_altmode_driver_api *)dev->api;

	return api->write(dev, reg, data);
}

__syscall int pd_altmode_isr_enable(const struct device *dev, bool en);

static inline int z_impl_pd_altmode_isr_enable(const struct device *dev,
					       bool en)
{
	const struct intel_altmode_driver_api *api =
		(const struct intel_altmode_driver_api *)dev->api;

	return api->isr_enable(dev, en);
}

__syscall int pd_altmode_is_interrupted(const struct device *dev);

static inline int z_impl_pd_altmode_is_interrupted(const struct device *dev)
{
	const struct intel_altmode_driver_api *api =
		(const struct intel_altmode_driver_api *)dev->api;

	return api->is_interrupted(dev);
}

__syscall void pd_altmode_set_result_cb(const struct device *dev,
					intel_altmode_callback cb);

static inline void z_impl_pd_altmode_set_result_cb(const struct device *dev,
						   intel_altmode_callback cb)
{
	const struct intel_altmode_driver_api *api =
		(const struct intel_altmode_driver_api *)dev->api;

	api->set_result_cb(dev, cb);
}

#include <syscalls/intel_altmode.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_INTEL_ALTMODE_H_ */
