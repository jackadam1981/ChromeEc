/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppc/ktu1125_public.h"

#define KTU1125_COMPAT kinetic_ktu1125

#define PPC_CHIP_KTU1125(id)                                                 \
	{                                                                    \
		.i2c_port = I2C_PORT_BY_DEV(id),                             \
		.i2c_addr_flags = DT_REG_ADDR(id), .drv = &ktu1125_drv,      \
		.frs_en = COND_CODE_1(                                       \
			DT_NODE_HAS_PROP(id, frs_alert_gpio),                \
			(GPIO_SIGNAL(DT_PHANDLE(id, frs_alert_gpio))), (0)), \
	}
