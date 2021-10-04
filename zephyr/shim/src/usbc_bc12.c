/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT pericom_pi3usb9201

#include <devicetree.h>
#include "bc12/pi3usb9201_public.h"
#include "usb_pd.h"

#define PORT0_NODE DT_PATH(usbc, usbc_port0)
#define PORT1_NODE DT_PATH(usbc, usbc_port1)

#if DT_NODE_EXISTS(PORT0_NODE)

#define PORT0_BC12_NODE DT_PATH(usbc, usbc_port0, bc12, port)

#if !DT_NODE_HAS_COMPAT(PORT0_BC12_NODE, pericom_pi3usb9201)
#error "Port0: invalid bc12 node in device tree"
#endif

#define PORT0_BC12_PORT_PHANDLE	DT_PHANDLE(PORT0_BC12_NODE, port)
#define PORT0_BC12_PORT		DT_STRING_UPPER_TOKEN(PORT0_BC12_PORT_PHANDLE, \
								enum_name)
#define PORT0_BC12_PORT_ADDR_FLAGS	DT_STRING_UPPER_TOKEN(PORT0_BC12_NODE, \
								i2c_addr_flags)
#endif /* DT_NODE_EXISTS(PORT0_NODE) */


#if DT_NODE_EXISTS(PORT1_NODE)

#define PORT1_BC12_NODE DT_PATH(usbc, usbc_port1, bc12, port)

#if !DT_NODE_HAS_COMPAT(PORT1_BC12_NODE, pericom_pi3usb9201)
#error "Port1: invalid bc12 node in device tree"
#endif

#define PORT1_BC12_PORT_PHANDLE	DT_PHANDLE(PORT1_BC12_NODE, port)
#define PORT1_BC12_PORT		DT_STRING_UPPER_TOKEN(PORT1_BC12_PORT_PHANDLE, \
								enum_name)
#define PORT1_BC12_PORT_ADDR_FLAGS	DT_STRING_UPPER_TOKEN(PORT1_BC12_NODE, \
								i2c_addr_flags)
#endif /* DT_NODE_EXISTS(PORT1_NODE) */

/* BC1.2 */
const struct pi3usb9201_config_t pi3usb9201_bc12_chips[] = {
	{
		.i2c_port = PORT0_BC12_PORT,
		.i2c_addr_flags = PORT0_BC12_PORT_ADDR_FLAGS,
	},
	{
		.i2c_port = PORT1_BC12_PORT,
		.i2c_addr_flags = PORT1_BC12_PORT_ADDR_FLAGS,
	},
};
