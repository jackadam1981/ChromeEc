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
	/* Set connection orientation */
	int (*set_connect_orient)(const struct emul *target,
				  const uint8_t orient);
	/* Set USB2 connection present */
	int (*connect_usb2)(const struct emul *target);
	/* Set USB3.2 connection present */
	int (*connect_usb3_2)(const struct emul *target);
	/* Set USB3.2 connection speed */
	int (*set_usb3_2_speed)(const struct emul *target, const uint8_t speed);
	/* Set Display Port connection present */
	int (*connect_dp)(const struct emul *target);
	/* Set Display Port IRQ */
	int (*set_dp_irq)(const struct emul *target);
	/* Set Hot Plug level */
	int (*set_hpd_lvl)(const struct emul *target);
	/* Set specific register value */
	int (*set_reg_value)(const struct emul *target, uint16_t reg,
			     const uint8_t *value, const uint8_t len);
	/* Reset all registers */
	int (*reset)(const struct emul *target);
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
emul_intel_pd_controller_set_connect_orient(const struct emul *target,
					    const uint8_t orient)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_connect_orient(target, orient);
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

static inline int
emul_intel_pd_controller_set_usb3_2_speed(const struct emul *target,
					  const uint8_t speed)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_usb3_2_speed(target, speed);
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

static inline int
emul_intel_pd_controller_set_reg_value(const struct emul *target, uint16_t reg,
				       const uint8_t *value, const uint8_t len)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->set_reg_value(target, reg, value, len);
}

static inline int emul_intel_pd_controller_reset(const struct emul *target)
{
	struct emul_intel_pd_controller_backend_api *backend_api =
		(struct emul_intel_pd_controller_backend_api *)
			target->backend_api;

	return backend_api->reset(target);
}
#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
