/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_INTEL_PD_H
#define __ZEPHYR_SHIM_INTEL_PD_H

#include <zephyr/devicetree.h>

#define INTEL_PD_COMPAT intel_pd

/* clang-format off */
#define INTEL_PD_CONFIG(id)                                            \
	{                                                              \
                .i2c_info = {                                          \
                        .port = I2C_PORT_BY_DEV(id),                   \
                        .addr_flags = DT_REG_ADDR(id),                 \
                },                                                     \
		.alert_signal = GPIO_SIGNAL(DT_PHANDLE(id, int_pin))   \
	}
/* clang-format on */

#define INTEL_PD_ARRAY(id) \
	[id] = INTEL_PD_CONFIG(id),

#define INTEL_PD_CONFIG_ARRAY  \
	DT_FOREACH_STATUS_OKAY(INTEL_PD_COMPAT, \
				INTEL_PD_ARRAY)

#endif /* __ZEPHYR_SHIM_INTEL_PD_H */
