/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Chrome OS-specific API for cec bitbang access
 * This exists only support the interface expected by the Chrome OS EC. It seems
 * better to implement this so we can make use of most of the existing code in
 * its cec_bitbang.c file and thus make sure we operate the same way.
 *
 * It provides raw access to cec bitbang timer and interrupt.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_CEC_BITBNAG_H_
#define ZEPHYR_INCLUDE_DRIVERS_CROS_CEC_BITBNAG_H_

#include "driver/cec/bitbang.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>

/**
 * @brief CROS CEC Bitbang Driver APIs
 * @defgroup cros_cec_bitbang_interface CROS cec bitbang Driver APIs
 * @ingroup io_interfaces
 * @{
 */

/**
 * @cond INTERNAL_HIDDEN
 *
 * cros cec bitbang driver API definition and system call entry points
 *
 * (Internal use only.)
 */
typedef int (*cros_cec_bitbang_api_tmr_cap_start)(const struct device *dev,
						  int port,
						  enum cec_cap_edge edge,
						  int timeout);

typedef int (*cros_cec_bitbang_api_tmr_cap_stop)(const struct device *dev,
						 int port);

typedef int (*cros_cec_bitbang_api_tmr_cap_get)(const struct device *dev,
						int port);

typedef int (*cros_cec_bitbang_api_debounce_enable)(const struct device *dev,
						    int port);

typedef int (*cros_cec_bitbang_api_debounce_disable)(const struct device *dev,
						     int port);

typedef int (*cros_cec_bitbang_api_trigger_send)(const struct device *dev,
						 int port);

typedef int (*cros_cec_bitbang_api_enable_timer)(const struct device *dev,
						 int port);

typedef int (*cros_cec_bitbang_api_disable_timer)(const struct device *dev,
						  int port);

typedef int (*cros_cec_bitbang_api_init_timer)(const struct device *dev,
					       int port);

__subsystem struct cros_cec_bitbang_driver_api {
	cros_cec_bitbang_api_tmr_cap_start tmr_cap_start;
	cros_cec_bitbang_api_tmr_cap_stop tmr_cap_stop;
	cros_cec_bitbang_api_tmr_cap_get tmr_cap_get;
	cros_cec_bitbang_api_debounce_enable debounce_enable;
	cros_cec_bitbang_api_debounce_disable debounce_disable;
	cros_cec_bitbang_api_trigger_send trigger_send;
	cros_cec_bitbang_api_enable_timer enable_timer;
	cros_cec_bitbang_api_disable_timer disable_timer;
	cros_cec_bitbang_api_init_timer init_timer;
};

/**
 * @endcond
 */

/**
 * @brief Initialize the cec bitbang interface.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_tmr_cap_start(const struct device *dev, int port,
					     enum cec_cap_edge edge,
					     int timeout);

static inline int
z_impl_cros_cec_bitbang_tmr_cap_start(const struct device *dev, int port,
				      enum cec_cap_edge edge, int timeout)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->tmr_cap_start) {
		return -ENOTSUP;
	}

	return api->tmr_cap_start(dev, port, edge, timeout);
}

/**
 * @brief Write to physical flash.
 *
 * Offset and size must be a multiple of CONFIG_FLASH_WRITE_SIZE.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param offset Flash offset to write.
 * @param size Number of bytes to write.
 * @param data Destination buffer for data.  Must be 32-bit aligned.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_tmr_cap_stop(const struct device *dev, int port);

static inline int z_impl_cros_cec_bitbang_tmr_cap_stop(const struct device *dev,
						       int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->tmr_cap_stop) {
		return -ENOTSUP;
	}

	return api->tmr_cap_stop(dev, port);
}

/**
 * @brief Erase physical flash.
 *
 * Offset and size must be a multiple of CONFIG_FLASH_ERASE_SIZE.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param offset	Flash offset to erase.
 * @param size	        Number of bytes to erase.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_tmr_cap_get(const struct device *dev, int port);

static inline int z_impl_cros_cec_bitbang_tmr_cap_get(const struct device *dev,
						      int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->tmr_cap_get) {
		return -ENOTSUP;
	}

	return api->tmr_cap_get(dev, port);
}

/**
 * @brief Read physical write protect setting for a flash bank.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param bank	Bank index to check.
 *
 * @return non-zero if bank is protected until reboot.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_debounce_enable(const struct device *dev,
					       int port);

static inline int
z_impl_cros_cec_bitbang_debounce_enable(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->debounce_enable) {
		return -ENOTSUP;
	}

	return api->debounce_enable(dev, port);
}

/* clang-format off */
/**
 * @brief Return flash protect state flags from the physical layer.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 *
 * @retval -ENOTSUP Not supported api function.
 */
__syscall
uint32_t cros_cec_bitbang_debounce_disable(const struct device *dev, int port);
/* clang-format on */

static inline uint32_t
z_impl_cros_cec_bitbang_debounce_disable(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->debounce_disable) {
		return -ENOTSUP;
	}

	return api->debounce_disable(dev, port);
}

/**
 * @brief Enable/disable protecting firmware/pstate at boot.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param new_flags	to protect (only EC_FLASH_PROTECT_*_AT_BOOT are
 * taken care of)
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_trigger_send(const struct device *dev, int port);

static inline int z_impl_cros_cec_bitbang_trigger_send(const struct device *dev,
						       int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->trigger_send) {
		return -ENOTSUP;
	}

	return api->trigger_send(dev, port);
}

/**
 * @brief Protect now physical flash.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param all	Protect all (=1) or just read-only and pstate (=0).
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_enable_timer(const struct device *dev, int port);

static inline int z_impl_cros_cec_bitbang_enable_timer(const struct device *dev,
						       int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->enable_timer) {
		return -ENOTSUP;
	}

	return api->enable_timer(dev, port);
}

/**
 * @brief Get JEDEC manufacturer and device identifiers.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param manufacturer Pointer to data where manufacturer id will be written.
 * @param device Pointer to data where device id will be written.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_disable_timer(const struct device *dev,
					     int port);

static inline int
z_impl_cros_cec_bitbang_disable_timer(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->disable_timer)
		return -ENOTSUP;

	return api->disable_timer(dev, port);
}

/**
 * @brief Get status registers.
 *
 * @param dev Pointer to the device structure for the flash driver instance.
 * @param sr1 Pointer to data where status1 register will be written.
 * @param sr2 Pointer to data where status2 register will be written.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cec_bitbang_init_timer(const struct device *dev, int port);

static inline int z_impl_cros_cec_bitbang_init_timer(const struct device *dev,
						     int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->init_timer)
		return -ENOTSUP;

	return api->init_timer(dev, port);
}

/**
 * @}
 */
#include <zephyr/syscalls/cros_cec_bitbang.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_CEC_BITBNAG_H_ */
