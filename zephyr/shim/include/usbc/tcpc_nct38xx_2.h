/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>
#include "driver/tcpm/nct38xx.h"

#define NCT38XX_TCPC_COMPAT1 nuvoton_nct38xx1

#define TCPC_CONFIG_NCT38XX1(id)                                               \
	{                                                                     \
		.bus_type = EC_BUS_TYPE_I2C,                                  \
		.i2c_info = {                                                 \
			.port = I2C_PORT(DT_PHANDLE(id, port)),               \
			.addr_flags = DT_STRING_UPPER_TOKEN(                  \
					id, i2c_addr_flags),                  \
		},                                                            \
		.drv = &nct38xx_tcpm_drv,                                     \
		.flags = TCPC_FLAGS_TCPCI_REV2_0 | 			      \
			TCPC_FLAGS_NO_DEBUG_ACC_CONTROL,		      \
	},


#define NCT38XX_TCPC_COMPAT2 nuvoton_nct38xx2

#define TCPC_CONFIG_NCT38XX2(id)                                               \
	{                                                                     \
		.bus_type = EC_BUS_TYPE_I2C,                                  \
		.i2c_info = {                                                 \
			.port = I2C_PORT(DT_PHANDLE(id, port)),               \
			.addr_flags = DT_STRING_UPPER_TOKEN(                  \
					id, i2c_addr_flags),                  \
		},                                                            \
		.drv = &nct38xx_tcpm_drv,                                     \
		.flags = TCPC_FLAGS_TCPCI_REV2_0,			      \
	},

