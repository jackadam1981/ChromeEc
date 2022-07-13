/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppc/ktu1125_public.h"

#define KTU1125_COMPAT kinetic_ktu1125

#define PPC_CHIP_KTU1125(id)                                                 \
	{			                                             \
		.i2c_port = I2C_PORT(DT_PHANDLE(id, port)),                  \
		.i2c_addr_flags = DT_STRING_UPPER_TOKEN(id, i2c_addr_flags), \
		.drv = &ktu1125_drv                                          \
	},
