/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/fusb302.h"

#include <zephyr/devicetree.h>

#define FUSB302_TCPC_COMPAT fairchild_fusb302

#define TCPC_CONFIG_FUSB302(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &fusb302_tcpm_drv,                                      \
		.int_cfg = GPIO_DT_SPEC_GET_OR(id, irq_gpios, {}),             \
	},

#define FUSB302_CHECK_FLAGS(id)                           \
	BUILD_ASSERT((DT_PROP(id, tcpc_flags) &           \
		      TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		     "incorrect tcpc interrupt configuration for FUSB302");

DT_FOREACH_STATUS_OKAY(FUSB302_TCPC_COMPAT, FUSB302_CHECK_FLAGS)
