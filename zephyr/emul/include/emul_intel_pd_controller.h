/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_PD_CONTROLLER_H
#define __EMUL_INTEL_PD_CONTROLLER_H

#include <zephyr/drivers/emul.h>

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in public documentation.
 */
__subsystem struct emul_intel_pd_controller_backend_api {
	/* Set data connection present */
	int (*connect_data)(const struct emul *target);
	/* Set USB2 connection present */
	int (*connect_usb2)(const struct emul *target);
	/* Set USB3.2 connection present */
	int (*connect_usb3_2)(const struct emul *target);
	/* Set Display Port connection present */
	int (*connect_dp)(const struct emul *target);
	/* Set Display Port IRQ */
	int (*set_dp_irq)(const struct emul *target);
	/* Set Hot Plug level */
	int (*set_hpd_lvl)(const struct emul *target);
};
/**
 * @endcond
 */

/**
 * @brief Establish PD controller connection.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int
emul_intel_pd_controller_connect_data(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_data(target);
}

/**
 * @brief Set connection to support USB2.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int
emul_intel_pd_controller_connect_usb2(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_usb2(target);
}

/**
 * @brief Set connection to support USB3.2.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int
emul_intel_pd_controller_connect_usb3_2(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_usb3_2(target);
}

/**
 * @brief Set connection to support DP.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int emul_intel_pd_controller_connect_dp(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_dp(target);
}

/**
 * @brief Assert DP Hot Plug Detect IRQ.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int emul_intel_pd_controller_set_dp_irq(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_dp_irq(target);
}

/**
 * @brief Set DP Hot Plug Detect high level.
 *
 * @param target The device Emulator instance.
 *
 * @retval 0 if success.
 * @retval -EIO general input/output error.
 */
static inline int
emul_intel_pd_controller_set_hpd_lvl(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_hpd_lvl(target);
}
#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
