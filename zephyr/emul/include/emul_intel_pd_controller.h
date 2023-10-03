/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_PD_CONTROLLER_H
#define __EMUL_INTEL_PD_CONTROLLER_H

#include <zephyr/drivers/emul.h>

__subsystem struct emul_intel_pd_controller_backend_api {
	/* Set data connection present */
	int (*connect_data)(const struct emul *emul);
	/* Set connection orientation */
	int (*set_connect_orient)(const struct emul *emul,
				  const uint8_t orient);
	/* Set USB2 connection present */
	int (*connect_usb2)(const struct emul *emul);
	/* Set USB3.2 connection present */
	int (*connect_usb3_2)(const struct emul *emul);
	/* Set USB3.2 connection speed */
	int (*set_usb3_2_speed)(const struct emul *emul, const uint8_t speed);
	/* Set Display Port connection present */
	int (*connect_dp)(const struct emul *emul);
	/* Set Display Port IRQ */
	int (*set_dp_irq)(const struct emul *emul);
	/* Set Hot Plug level */
	int (*set_hpd_lvl)(const struct emul *emul);
	/* Set specific register value */
	int (*set_reg_value)(const struct emul *emul, uint16_t reg,
			     const uint8_t *value, const uint8_t len);
	/* Reset all registers */
	int (*reset)(const struct emul *emul);
};

#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
