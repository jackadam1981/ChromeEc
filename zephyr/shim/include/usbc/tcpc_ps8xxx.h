/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/ps8xxx_public.h"

#include <zephyr/devicetree.h>

#define PS8XXX_COMPAT parade_ps8xxx

#define OUR_DT_SPEC(id)                                         \
	{                                                       \
		.port = DEVICE_DT_GET(DT_GPIO_CTLR(id, gpios)), \
		.pin = DT_GPIO_PIN(id, gpios),                  \
		.dt_flags = 0xFF & (DT_GPIO_FLAGS(id, gpios)),  \
	}

#define TCPC_CONFIG_PS8XXX(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &ps8xxx_tcpm_drv,                                       \
		.flags = DT_PROP(id, tcpc_flags),                              \
		COND_CODE_1(DT_NODE_HAS_PROP(id, int_pin),                     \
			    (.int_cfg = OUR_DT_SPEC(DT_PHANDLE(id, int_pin)),  \
			    ), ())                                             \
	},

#define PS8XXX_CHECK_FLAGS(id)                            \
	BUILD_ASSERT((DT_PROP(id, tcpc_flags) &           \
		      TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		     "incorrect tcpc interrupt configuration for PS8XXX");

DT_FOREACH_STATUS_OKAY(PS8XXX_COMPAT, PS8XXX_CHECK_FLAGS)
