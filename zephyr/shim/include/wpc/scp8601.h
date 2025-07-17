/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/wpc/cps8100.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CPS8601_PCHG_COMPAT convenientpower_scp8601

#define WPC_CHIP_CPS8601(id) \
	{								\
		.cfg = &(const struct pchg_config) {                    \
			.i2c_port = I2C_PORT_BY_DEV(node_id),           \
			.i2c_addr_flags = I2C_ADDR_FLAGS(DT_REG_ADDR(node_id)), \
			.drv = &cps8601_drv,                             \
			.irq_gpio = GPIO_DT_SPEC_GET(node_id, irq_gpios, {}), \
			.full_percent = DT_PROP(node_id, full_percent),  \
			.block_size = DT_PROP(node_id, block_size),      \
			.flags = PCHG_CFG_FW_UPDATE_SYNC,                \
		},\
		.policy = {\
			[PCHG_CHIPSET_STATE_ON] = &pchg_policy_on,\
			[PCHG_CHIPSET_STATE_SUSPEND] = &pchg_policy_suspend,\
		},\
		.events = QUEUE_NULL(PCHG_EVENT_QUEUE_SIZE, enum pchg_event),\
	}
#ifdef __cplusplus
}
#endif
