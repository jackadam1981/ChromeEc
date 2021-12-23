/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "driver/charger/isl923x.h"

const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_USB_C0_TCPC,
		.i2c_addr_flags = ISL923X_ADDR_FLAGS,
		.drv = &isl923x_drv,
	},
	/* TODO(b:211693800) port 1 is present on sub-boards 1 and 2 with same
	 * configuration as port 0 but on I2C_PORT_USB_C1_TCPC.
	 */
};
const unsigned int chg_cnt = ARRAY_SIZE(chg_chips);
