/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpm/rt1718s_public.h"

#include <zephyr/devicetree.h>

#define RT1718S_TCPC_COMPAT richtek_rt1718s_tcpc

#define INT_PIN_CONFIG_RT1718S(id)                                           \
	COND_CODE_1(DT_NODE_HAS_PROP(id, int_pin),                           \
		    (.gpio_port = DEVICE_DT_GET(                             \
			     DT_GPIO_CTLR(DT_PHANDLE(id, int_pin), gpios)),  \
		     .interrupt_pin =                                        \
			     DT_GPIO_PIN(DT_PHANDLE(id, int_pin), gpios), ), \
		    ())

#define TCPC_CONFIG_RT1718S(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &rt1718s_tcpm_drv,                                      \
		.flags = DT_PROP(id, tcpc_flags),                              \
		INT_PIN_CONFIG_RT17118S(id)                                    \
	},

#define RT17118S_CHECK_FLAGS(id) \
	BUILD_ASSERT(            \
		(DT_PROP(id, tcpc_flags) & TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0)

DT_FOREACH_STATUS_OKAY(RT1718S_TCPC_COMPAT, RT17118S_CHECK_FLAGS)
