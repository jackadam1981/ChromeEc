/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/tcpci.h"
#include "usbc/tcpc_anx7447.h"
#include "usbc/tcpc_anx7447_emul.h"
#include "usbc/tcpc_ccgxxf.h"
#include "usbc/tcpc_fusb302.h"
#include "usbc/tcpc_generic_emul.h"
#include "usbc/tcpc_nct38xx.h"
#include "usbc/tcpc_ps8xxx.h"
#include "usbc/tcpc_ps8xxx_emul.h"
#include "usbc/tcpc_raa489000.h"
#include "usbc/tcpc_rt1715.h"
#include "usbc/tcpc_rt1718s.h"
#include "usbc/utils.h"

#include <zephyr/devicetree.h>

#define TCPCI_COMPAT cros_ec_tcpci

/* clang-format off */
#define TCPC_CONFIG_TCPCI(id)                            \
	{                                                \
		.bus_type = EC_BUS_TYPE_I2C,           \
		.i2c_info = {                          \
			.port = I2C_PORT_BY_DEV(id),   \
			.addr_flags = DT_REG_ADDR(id), \
		},                                     \
		.drv = &tcpci_tcpm_drv, \
	}
/* clang-format on */

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
DT_FOREACH_STATUS_OKAY(ANX7447_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(CCGXXF_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(FUSB302_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(PS8XXX_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(NCT38XX_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(RAA489000_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(RT1718S_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(RT1715_TCPC_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(TCPCI_COMPAT, TCPC_ALT_DECLARE)

#ifdef TEST_BUILD
DT_FOREACH_STATUS_OKAY(TCPCI_EMUL_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(PS8XXX_EMUL_COMPAT, TCPC_ALT_DECLARE)
DT_FOREACH_STATUS_OKAY(ANX7447_EMUL_COMPAT, TCPC_ALT_DECLARE)
#endif

#define TCPC_ENABLE_ALTERNATE_BY_NODELABEL(usb_port_num, nodelabel) \
	memcpy(&tcpc_config[usb_port_num],                          \
	       &TCPC_ALT_FROM_NODELABEL(nodelabel),                 \
	       sizeof(struct tcpc_config_t))
