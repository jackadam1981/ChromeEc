/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/tcpci.h"
#include "usbc/tcpc_rt171x.h"

#include <zephyr/devicetree.h>

#define TCPCI_COMPAT cros_ec_tcpci

#define TCPC_CONFIG_TCPCI(id) \
	{                                              \
		.bus_type = EC_BUS_TYPE_I2C,           \
		.i2c_info = {                          \
			.port = I2C_PORT_BY_DEV(id),   \
			.addr_flags = DT_REG_ADDR(id), \
		},                                     \
		.drv = &tcpci_tcpm_drv,                \
	},

#define TCPC_ALT_NAME_GET(node_id) DT_CAT(tcpc_alt_, node_id)

#define TCPC_ALT_FROM_NODELABEL(lbl) (TCPC_ALT_NAME_GET(DT_NODELABEL(lbl)))

#define TCPC_ALT_DECLARATION(node_id) \
	extern const struct tcpc_config_t TCPC_ALT_NAME_GET(node_id)

#define TCPC_ALT_DECLARE(node_id)                   \
	COND_CODE_1(DT_PROP_OR(node_id, is_alt, 0), \
		    (TCPC_ALT_DECLARATION(node_id);), ())

/*
 * Forward declare a struct tcpc_config_t for every TCPC node in the tree with
 * the "is-alt" property set.
 */
DT_FOREACH_STATUS_OKAY(RT171X_TCPC_COMPAT, TCPC_ALT_DECLARE)

#define TCPC_ENABLE_ALTERNATE_BY_NODELABEL(usb_port_num, nodelabel) \
	memcpy(&tcpc_config[usb_port_num],                          \
	       &TCPC_ALT_FROM_NODELABEL(nodelabel),                 \
	       sizeof(struct tcpc_config_t))
