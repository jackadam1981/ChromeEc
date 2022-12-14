/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TCPC_NCT38XX_H
#define __CROS_EC_TCPC_NCT38XX_H

#include "driver/tcpm/nct38xx.h"

#include <zephyr/devicetree.h>

#define NCT38XX_TCPC_COMPAT nuvoton_nct38xx

#define INT_PIN_CONFIG_NCT38XX(id)                                           \
	COND_CODE_1(DT_NODE_HAS_PROP(id, int_pin),                           \
		    (.gpio_port = DEVICE_DT_GET(                             \
			     DT_GPIO_CTLR(DT_PHANDLE(id, int_pin), gpios)),  \
		     .interrupt_pin =                                        \
			     DT_GPIO_PIN(DT_PHANDLE(id, int_pin), gpios), ), \
		    ())

#define TCPC_CONFIG_NCT38XX(id) \
	{                                                                      \
		.bus_type = EC_BUS_TYPE_I2C,                                   \
		.i2c_info = {                                                  \
			.port = I2C_PORT_BY_DEV(id),                           \
			.addr_flags = DT_REG_ADDR(id),                         \
		},                                                             \
		.drv = &nct38xx_tcpm_drv,                                      \
		.flags = DT_PROP(id, tcpc_flags),                              \
		INT_PIN_CONFIG_NCT38XX(id)                                     \
	},

/**
 * @brief Get the NCT38XX GPIO device from the TCPC port enumeration
 *
 * @param port The enumeration of TCPC port
 *
 * @return NULL if failed, otherwise a pointer to NCT38XX GPIO device
 */
const struct device *nct38xx_get_gpio_device_from_port(const int port);

#define NCT38XX_CHECK_FLAGS(id)                           \
	BUILD_ASSERT((DT_PROP(id, tcpc_flags) &           \
		      TCPC_FLAGS_ALERT_ACTIVE_HIGH) == 0, \
		     "incorrect tcpc interrupt configuration for NCT38XX");

DT_FOREACH_STATUS_OKAY(NCT38XX_TCPC_COMPAT, NCT38XX_CHECK_FLAGS)

#endif /* __CROS_EC_TCPC_NCT38XX_H */
