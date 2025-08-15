/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/charger/isl95522_public.h"

#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ISL95522_CHG_COMPAT intersil_isl95522

#define CHG_CONFIG_ISL95522(id)                    \
	{                                          \
		.i2c_port = I2C_PORT_BY_DEV(id),   \
		.i2c_addr_flags = DT_REG_ADDR(id), \
		.drv = &isl95522_drv,              \
	},

#ifdef __cplusplus
}
#endif
