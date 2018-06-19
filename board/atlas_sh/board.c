/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atlas TP configuration */

#include "console.h"
#include "i2c.h"
#include "registers.h"

/* GPIOs */
#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

/* I2C port map */
const struct i2c_port_t i2c_ports[]  = {
	{"trackpad", I2C_PORT_TP,   400, GPIO_I2C0_SCL,   GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/*
 * Dummy functions to remove 'undefined' symbol link error for acpi.o
 * due to CONFIG_HOSTCMD_LPC flag
 */
int lpc_query_host_event_state(void)
{
	return 0;
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
}
