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

void ppc_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_SWCTL_INT_ODL:
		sn5s330_interrupt(0);
		break;
	case GPIO_USB_C1_SWCTL_INT_ODL:
		sn5s330_interrupt(1);
		break;
	default:
		break;
	}
}

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0,
		"No compatible PPC instance found");

#define USBC_PORT_PPC(inst)                                                   \
	{                                                                     \
		.i2c_port = DT_STRING_UPPER_TOKEN(DT_PHANDLE(                 \
					DT_DRV_INST(inst), port), enum_name), \
		.i2c_addr_flags = DT_STRING_UPPER_TOKEN(                      \
					DT_DRV_INST(inst), i2c_addr_flags),   \
		.drv = &sn5s330_drv                                           \
	},

/* Power Path Controller */
struct ppc_config_t ppc_chips[] = {
#if 1
	DT_INST_FOREACH_STATUS_OKAY(USBC_PORT_PPC)
#else
	{
		.i2c_port = I2C_PORT_TCPC0,
		.i2c_addr_flags = SN5S330_ADDR0_FLAGS,
		.drv = &sn5s330_drv
	},
	{
		.i2c_port = I2C_PORT_TCPC1,
		.i2c_addr_flags = SN5S330_ADDR0_FLAGS,
		.drv = &sn5s330_drv
	},
#endif
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);
