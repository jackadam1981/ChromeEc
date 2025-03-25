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
typedef void (*cros_cec_bitbang_api_tmr_cap_start)(const struct device *dev,
						   int port,
						   enum cec_cap_edge edge,
						   int timeout);

typedef void (*cros_cec_bitbang_api_tmr_cap_stop)(const struct device *dev,
						  int port);

typedef int (*cros_cec_bitbang_api_tmr_cap_get)(const struct device *dev,
						int port);

typedef void (*cros_cec_bitbang_api_debounce_enable)(const struct device *dev,
						     int port);

typedef void (*cros_cec_bitbang_api_debounce_disable)(const struct device *dev,
						      int port);

typedef void (*cros_cec_bitbang_api_trigger_send)(const struct device *dev,
						  int port);

typedef void (*cros_cec_bitbang_api_enable_timer)(const struct device *dev,
						  int port);

typedef void (*cros_cec_bitbang_api_disable_timer)(const struct device *dev,
						   int port);

typedef void (*cros_cec_bitbang_api_init_timer)(const struct device *dev,
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
 * @brief Start the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 * @param edge to trigger capture timer interrupt on
 * @param timeout timeout for capture interrupt edge
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_tmr_cap_start(const struct device *dev,
					      int port, enum cec_cap_edge edge,
					      int timeout);

static inline void
z_impl_cros_cec_bitbang_tmr_cap_start(const struct device *dev, int port,
				      enum cec_cap_edge edge, int timeout)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->tmr_cap_start) {
		return;
	}

	api->tmr_cap_start(dev, port, edge, timeout);
}

/**
 * @brief Stop the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_tmr_cap_stop(const struct device *dev,
					     int port);

static inline void
z_impl_cros_cec_bitbang_tmr_cap_stop(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->tmr_cap_stop) {
		return;
	}

	api->tmr_cap_stop(dev, port);
}

/**
 * @brief get the time from the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return time read from the timer
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
 * @brief enter debounce state.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_debounce_enable(const struct device *dev,
						int port);

static inline void
z_impl_cros_cec_bitbang_debounce_enable(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->debounce_enable) {
		return;
	}

	api->debounce_enable(dev, port);
}

/**
 * @brief leave debounce state.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_debounce_disable(const struct device *dev,
						 int port);
/* clang-format on */

static inline void
z_impl_cros_cec_bitbang_debounce_disable(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->debounce_disable) {
		return;
	}

	api->debounce_disable(dev, port);
}

/**
 * @brief Elevate to interrupt context.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_trigger_send(const struct device *dev,
					     int port);

static inline void
z_impl_cros_cec_bitbang_trigger_send(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->trigger_send) {
		return;
	}

	api->trigger_send(dev, port);
}

/**
 * @brief enable the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_enable_timer(const struct device *dev,
					     int port);

static inline void
z_impl_cros_cec_bitbang_enable_timer(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->enable_timer) {
		return;
	}

	api->enable_timer(dev, port);
}

/**
 * @brief disable the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_disable_timer(const struct device *dev,
					      int port);

static inline void
z_impl_cros_cec_bitbang_disable_timer(const struct device *dev, int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->disable_timer)
		return;

	api->disable_timer(dev, port);
}

/**
 * @brief initialize the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall void cros_cec_bitbang_init_timer(const struct device *dev, int port);

static inline void z_impl_cros_cec_bitbang_init_timer(const struct device *dev,
						      int port)
{
	const struct cros_cec_bitbang_driver_api *api =
		(const struct cros_cec_bitbang_driver_api *)dev->api;

	if (!api->init_timer)
		return;

	api->init_timer(dev, port);
}

/**
 * @}
 */
#include <zephyr/syscalls/cros_cec_bitbang.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_CEC_BITBNAG_H_ */
