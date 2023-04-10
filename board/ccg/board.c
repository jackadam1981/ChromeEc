/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADLRVP-ITE board-specific configuration */

#include "i2c.h"
#include "intc.h"
#include "ioexpander.h"
#include "pca9535.h"
#include "registers.h"

const struct i2c_port_t i2c_ports[] = {
       [0] = {
                .name = "batt_chg",
                .port = IT83XX_I2C_CH_B,
                .kbps = 100,
                .scl = GPIO_SMB_BS_CLK,
                .sda = GPIO_SMB_BS_DATA,
        },
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

struct ioexpander_config_t ioex_config[] = {
	[0] = {
		.i2c_host_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = 0x23,
		.drv = &pca9535_ioexpander_drv,
	},
};

#include "gpio_list.h"
/******************************************************************************/
