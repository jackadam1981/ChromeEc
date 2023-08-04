/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pd_task_intel_altmode.h"
#include "usbc/utils.h"

#define INTEL_ALTMODE_COMPAT_PD intel_pd_altmode

#define INTEL_ALTMODE_PD_CONFIG(id)                                    \
	{                                                              \
		.i2c_info = {                                        \
			.port = I2C_PORT_BY_DEV(id),                 \
			.addr_flags = DT_REG_ADDR(id),               \
		},                                                   \
		.alert_signal = GPIO_SIGNAL(DT_PHANDLE(id, int_pin)) \
	}

#define PD_CHIP_ENTRY(usbc_id, pd_id, config_fn) \
	[USBC_PORT_NEW(usbc_id)] = config_fn(pd_id),

#define CHECK_COMPAT(compat, usbc_id, pd_id, config_fn) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(pd_id, compat),  \
		    (PD_CHIP_ENTRY(usbc_id, pd_id, config_fn)), ())

#define PD_CHIP_FIND(usbc_id, pd_id)                          \
	CHECK_COMPAT(INTEL_ALTMODE_COMPAT_PD, usbc_id, pd_id, \
		     INTEL_ALTMODE_PD_CONFIG)

#define PD_CHIP(usbc_id)                                                      \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, pd_altmode),                    \
		    (PD_CHIP_FIND(usbc_id, DT_PHANDLE(usbc_id, pd_altmode))), \
		    ())

const struct pd_config_t pd_config[] = { DT_FOREACH_STATUS_OKAY(named_usbc_port,
								PD_CHIP) };

BUILD_ASSERT(ARRAY_SIZE(pd_config) == CONFIG_USB_PD_PORT_MAX_COUNT);
