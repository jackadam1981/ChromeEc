/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpm/rt1718s_public.h"

#include <zephyr/devicetree.h>

#define RT1718S_TCPC_COMPAT richtek_rt1718s_tcpc

#define TCPC_CONFIG_RT1718S(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &rt1718s_tcpm_drv,                                      \
		.flags = DT_PROP(id, tcpc_flags),                              \
		.int_cfg = GPIO_DT_SPEC_GET_OR(id, irq_gpios, {}),             \
	},

#define RT17118S_CHECK_FLAGS(id)                          \
	BUILD_ASSERT((DT_PROP(id, tcpc_flags) &           \
		      TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		     "incorrect tcpc interrupt configuration for RT1718S");

DT_FOREACH_STATUS_OKAY(RT1718S_TCPC_COMPAT, RT17118S_CHECK_FLAGS)
