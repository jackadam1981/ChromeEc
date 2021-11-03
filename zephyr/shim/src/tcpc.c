/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <devicetree.h>

#include "config.h"
#include "usbc/tcpc.h"

#define TCPC_PORT(id) DT_REG_ADDR(DT_PARENT(id))

#define TCPC_DEV_WITH_COMMA(id) DEVICE_DT_GET(DT_PHANDLE(id, tcpc_port)),

#define TCPC_DEV_BINDING(id)                         \
	COND_CODE_1(DT_NODE_HAS_PROP(id, tcpc_port), \
		    ([TCPC_PORT(id)] = TCPC_DEV_WITH_COMMA(id)), ())

/* device pool for binding the port index and TCPC device */
static const struct device
	*tcpc_devices[CONFIG_PLATFORM_EC_USB_PD_PORT_MAX_COUNT] = {
		DT_FOREACH_STATUS_OKAY(nuvoton_nct38xx, TCPC_DEV_BINDING)
	};

const struct device *tcpc_get_device_from_port(const int port)
{
	if (port < 0 || port >= CONFIG_PLATFORM_EC_USB_PD_PORT_MAX_COUNT)
		return NULL;
	return tcpc_devices[port];
}
