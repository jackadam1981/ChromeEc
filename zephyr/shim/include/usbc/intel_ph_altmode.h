/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_INTEL_PD_ALTMODE_H
#define __ZEPHYR_SHIM_INTEL_PD_ALTMODE_H

#include <zephyr/devicetree.h>

/* clang-format off */
#define INTEL_ALTMODE_COMPAT_PD intel_pd-altmode
/* clang-format on */

#define INTEL_ALTMODE_PD_CONFIG(id)                                    \
	{                                                              \
		.i2c_info = {                                        \
			.port = I2C_PORT_BY_DEV(id),                 \
			.addr_flags = DT_REG_ADDR(id),               \
		},                                                   \
		.alert_signal = GPIO_SIGNAL(DT_PHANDLE(id, int_pin)) \
	}

#define INTEL_ALTMODE_PD_ARRAY(id) [id] = INTEL_ALTMODE_PD_CONFIG(id),

#define INTEL_ALTMODE_PD_CONFIG_ARRAY \
	DT_FOREACH_STATUS_OKAY(INTEL_ALTMODE_COMPAT_PD, INTEL_ALTMODE_PD_ARRAY)

#endif /* __ZEPHYR_SHIM_INTEL_PD_H */
