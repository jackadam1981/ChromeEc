/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_tcpc

#include <devicetree.h>
#include "hooks.h"
#include "usb_pd_tcpm.h"
#include "usb_pd.h"

/* Include drivers if enabled */
#if defined(CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8705)     || \
	defined(CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8751) || \
	defined(CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8755) || \
	defined(CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8805) || \
	defined(CONFIG_PLATFORM_EC_USB_PD_TCPM_PS8815)
#include "driver/tcpm/ps8xxx_public.h"
#endif
#ifdef CONFIG_PLATFORM_EC_USB_PD_TCPM_FUSB302
#include "fusb302.h"
#endif


#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0,
		"No compatible TCPC instance found");

#define USBC_PORT_TCPC(inst)                                                  \
	{                                                                     \
		.bus_type = EC_BUS_TYPE_I2C,                                  \
		.i2c_info = {                                                 \
			.port = I2C_PORT(DT_PHANDLE(                          \
					DT_DRV_INST(inst), port)),            \
			.addr_flags = DT_PROP(DT_DRV_INST(inst),              \
						i2c_addr_flags),              \
		},                                                            \
		.drv = &DT_STRING_TOKEN(DT_DRV_INST(inst), drv),              \
		.flags = DT_PROP(DT_DRV_INST(inst), flags),                   \
	},                                                                    \

const struct tcpc_config_t tcpc_config[] = {
	DT_INST_FOREACH_STATUS_OKAY(USBC_PORT_TCPC)
};

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
