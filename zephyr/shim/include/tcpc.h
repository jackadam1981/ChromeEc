/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TCPC_H
#define __CROS_EC_TCPC_H

#include <device.h>
#include <devicetree.h>

#if DT_NODE_EXISTS(DT_PATH(named_tcpc_ports))

#define TCPC_PORT(id) DT_STRING_UPPER_TOKEN(id, enum_name)
#define TCPC_PORT_WITH_COMMA(id) TCPC_PORT(id),

enum tcpc_ports {
	DT_FOREACH_CHILD(DT_PATH(named_tcpc_ports), TCPC_PORT_WITH_COMMA)
	TCPC_PORT_COUNT
};

#endif /* named_tcpc_ports */

const struct device *tcpc_get_device_from_port(const int port);

#endif /* __CROS_EC_TCPC_H */
