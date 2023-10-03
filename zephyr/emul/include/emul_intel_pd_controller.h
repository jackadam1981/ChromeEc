/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_PD_CONTROLLER_H
#define __EMUL_INTEL_PD_CONTROLLER_H

#include <zephyr/drivers/emul.h>

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

static inline int
emul_intel_pd_controller_connect_data(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_data(target);
}

static inline int
emul_intel_pd_controller_connect_usb2(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_usb2(target);
}

static inline int
emul_intel_pd_controller_connect_usb3_2(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_usb3_2(target);
}

static inline int emul_intel_pd_controller_connect_dp(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->connect_dp(target);
}

static inline int emul_intel_pd_controller_set_dp_irq(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_dp_irq(target);
}

static inline int
emul_intel_pd_controller_set_hpd_lvl(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_hpd_lvl(target);
}
#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
