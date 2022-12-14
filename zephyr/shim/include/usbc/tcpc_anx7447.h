/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpm/anx7447_public.h"

#include <zephyr/devicetree.h>

#define ANX7447_TCPC_COMPAT analogix_anx7447_tcpc

#define INT_PIN_CONFIG_ANX7447(id)                                           \
	COND_CODE_1(DT_NODE_HAS_PROP(id, int_pin),                           \
		    (.gpio_port = DEVICE_DT_GET(                             \
			     DT_GPIO_CTLR(DT_PHANDLE(id, int_pin), gpios)),  \
		     .interrupt_pin =                                        \
			     DT_GPIO_PIN(DT_PHANDLE(id, int_pin), gpios), ), \
		    ())

#define TCPC_CONFIG_ANX7447(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &anx7447_tcpm_drv,                                      \
		.flags = DT_PROP(id, tcpc_flags),                              \
		INT_PIN_CONFIG_ANX7447(id)                                     \
	},
