/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Soraka ISH board-specific configuration */

#include "als.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/baro_bmp280.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense.h"
#include "task.h"
#include "uart.h"

#include "gpio_list.h" /* has to be included last */
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/* I2C port map */
const struct i2c_port_t i2c_ports[]  = {
	{"trackpad", I2C_PORT_TP,   400, GPIO_I2C0_SCL,   GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* dummy functions to remove 'undefined' symbol link error for acpi.o
 * due to CONFIG_LPC flag
 */
#ifdef CONFIG_LPC
int lpc_query_host_event_state(void)
{
	return 0;
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
}
#endif
