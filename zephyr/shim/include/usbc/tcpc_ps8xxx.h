/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/ps8xxx_public.h"

#include <zephyr/devicetree.h>

#define PS8XXX_COMPAT parade_ps8xxx

#define TCPC_CONFIG_PS8XXX(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &ps8xxx_tcpm_drv,                                       \
		.flags = DT_PROP(id, tcpc_flags),                              \
		.int_cfg = GPIO_DT_SPEC_GET_OR(id, irq_gpios, {}),             \
	},

#define PS8XXX_CHECK_FLAGS(id)                            \
	BUILD_ASSERT((DT_PROP(id, tcpc_flags) &           \
		      TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		     "incorrect tcpc interrupt configuration for PS8XXX")

DT_FOREACH_STATUS_OKAY(PS8XXX_COMPAT, PS8XXX_CHECK_FLAGS;)
