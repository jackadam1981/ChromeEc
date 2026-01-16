/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#ifdef CONFIG_PLATFORM_EC_CHARGER_RT9479
#include "driver/charger/rt9479.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RT9479_CHG_COMPAT richtek_rt9479

#define CHG_CONFIG_RT9479(id)                      \
	{                                          \
		.i2c_port = I2C_PORT_BY_DEV(id),   \
		.i2c_addr_flags = DT_REG_ADDR(id), \
		.drv = &rt9479_drv,                \
	},

#ifdef __cplusplus
}
#endif

#endif
