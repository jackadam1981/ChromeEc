/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ti_sn5s330

#include <devicetree.h>
#include "ppc/sn5s330_public.h"
#include "usb_pd.h"
#include "usbc_ocp.h"
#include "usbc_ppc.h"

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0,
		"No compatible PPC instance found");

#define USBC_PORT_PPC(inst)                                                   \
	{                                                                     \
		.i2c_port = I2C_PORT(DT_PHANDLE(DT_DRV_INST(inst), port)),    \
		.i2c_addr_flags = DT_STRING_UPPER_TOKEN(                      \
					DT_DRV_INST(inst), i2c_addr_flags),   \
		.drv = &sn5s330_drv                                           \
	},

/* Power Path Controller */
struct ppc_config_t ppc_chips[] = {
	DT_INST_FOREACH_STATUS_OKAY(USBC_PORT_PPC)
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

#define MK_CHIP_ENTRY(
		id, s_chip, s_type, s_drv, s_min_freq, s_max_freq)	\
	[SENSOR_ID(id)] = {						\
		SENSOR_BASIC_INFO(id)					\
		.chip = s_chip,						\
		.type = s_type,						\
		.drv = &s_drv,						\
		.min_frequency = s_min_freq,				\
		.max_frequency = s_max_freq				\
	},

#define CREATE_PPC_DRIVER(s_compat, ...) \
    UTIL_LISTIFY(/* num_chips */

struct ppc_config_t ppc_chips[] = {
#if DT_NODE_EXISTS(SENSOR_NODE)
#include "ppc_driver/ppc_drv_list.inc"
#endif
};
