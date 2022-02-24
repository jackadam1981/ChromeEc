/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADLRVP-NPCX board-specific configuration */
#include "adlrvp_zephyr.h"
#include "button.h"
#include "fusb302.h"
#include "i2c_bitbang.h"
#include "lid_switch.h"
#include "pca9675.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "tablet_mode.h"
#include "uart.h"
#include "usb_pd_tcpm.h"

/* USB-C TCPC Configuration */
const struct tcpc_config_t tcpc_config[] = {
	[TYPE_C_PORT_0] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC_0,
			.addr_flags = I2C_ADDR_FUSB302_TCPC_AIC,
		},
		.drv = &fusb302_tcpm_drv,
	},
#if defined(HAS_TASK_PD_C1)
	[TYPE_C_PORT_1] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC_1,
			.addr_flags = I2C_ADDR_FUSB302_TCPC_AIC,
		},
		.drv = &fusb302_tcpm_drv,
	},
#endif
#if defined(HAS_TASK_PD_C2)
	[TYPE_C_PORT_2] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC_2,
			.addr_flags = I2C_ADDR_FUSB302_TCPC_AIC,
		},
		.drv = &fusb302_tcpm_drv,
	},
#endif
#if defined(HAS_TASK_PD_C3)
	[TYPE_C_PORT_3] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TYPEC_3,
			.addr_flags = I2C_ADDR_FUSB302_TCPC_AIC,
		},
		.drv = &fusb302_tcpm_drv,
	},
#endif
};
BUILD_ASSERT(ARRAY_SIZE(tcpc_config) == CONFIG_USB_PD_PORT_MAX_COUNT);
