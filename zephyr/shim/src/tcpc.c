/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpc.h"

/*
 * The named-tcpc-ports node is required by the TCPC shim
 */
#if !DT_NODE_EXISTS(DT_PATH(named_tcpc_ports))
#error TCPC shim requires the named-tcpc-ports node to be defined.
#endif

#define INIT_DEV_BINDING(id) \
	[TCPC_PORT(id)] = DEVICE_DT_GET(DT_PHANDLE(id, tcpc_port)),

/*
 * Long term we will not need these, for now they're needed those for shim
 */
static const struct device *tcpc_devices[TCPC_PORT_COUNT] = {
	DT_FOREACH_CHILD(DT_PATH(named_tcpc_ports), INIT_DEV_BINDING)
};

const struct device *tcpc_get_device_from_port(const int port)
{
	if (port < 0 || port >= TCPC_PORT_COUNT)
		return NULL;
	return tcpc_devices[port];
}
